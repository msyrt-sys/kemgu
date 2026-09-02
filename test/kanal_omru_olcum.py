# [D-540] Kanal ömrü seçenek A'nın ÖN KOŞUL 1 ölçümü.
#
# SORU: "kanalı yakalayan TÜM görevler aynı işlevde birleştiriliyor" şekli
# gerçek kodda kaç kez geçiyor? Tavan sıfıra yakınsa A ölçülemez bir
# değişiklik olurdu (D-430) ve yazılmamalıydı.
#
# ⚠ GREP KULLANILMAZ — D-532'de tam bu hata yapıldı: grep YORUMLARI ve
#   dizgileri de sayar. Ölçüm `kemgu --ast` çıktısı üzerinden yapılır;
#   `selfhost/*.kem` grep'te eşleşir ama AST'de HİÇBİR kanal bağlaması
#   yoktur (adlar yalnız derleyicinin kendi dizgilerinde geçer).
#
# ⚠ ÖLÇÜM ARACININ KENDİSİ ÖNCE YANLIŞTI (D-500): ilk sürüm `gönderen(k)` /
#   `alan(k)` PROJEKSİYONLARINI izlemiyordu ve `kanal_mesaj` için
#   `yakalayan=0` dedi — oysa D-511 orada kanalın göreve YAKALANDIĞINI
#   ölçmüştü. Çelişki aracı şüpheli kıldı; takma ad izleme eklenince
#   `yakalayan=1` çıktı ve iki bağımsız ölçüm uyuştu.
#
# BİLİNEN SINIRLAR (dürüstçe): işlevler arası kanal geçişi izlenmez ·
# birleştirmeler görev-tutamağı başına DEĞİL işlev genelinde sayılır ·
# yakalama testi sözdizimseldir (spawn alt-ağacında adın geçmesi).
#
# Koşum: python3 test/kanal_omru_olcum.py  (WSL — Windows'ta python3 YOK)
#        Önce: grep -rl "kanal_oluştur" --include=*.kem . > build/kanal/aday.txt
import subprocess, os
os.chdir(os.path.expanduser("~/kemgu"))
dosyalar = [l.strip() for l in open("build/kanal/aday.txt") if l.strip()]
def agac(dosya):
    r = subprocess.run(["./build/kemgu","--ast",dosya], capture_output=True, text=True, errors="replace")
    if r.returncode != 0: return None
    dugum=[]
    for satir in r.stdout.splitlines():
        p = satir.split("\t")
        if len(p) < 3: continue
        try: d = int(p[0])
        except: continue
        dugum.append((d, p[1], p[2]))
    cocuk=[[] for _ in dugum]; yigin=[]
    for i,(d,_,_) in enumerate(dugum):
        while yigin and dugum[yigin[-1]][0] >= d: yigin.pop()
        if yigin: cocuk[yigin[-1]].append(i)
        yigin.append(i)
    return dugum, cocuk
def altagac(cocuk, k):
    y=[k]; hepsi=[]
    while y:
        n=y.pop(); hepsi.append(n); y.extend(cocuk[n])
    return hepsi
def cagri_adi(dugum, cocuk, n):
    if dugum[n][1]!="CAGRI" or not cocuk[n]: return None
    ilk=cocuk[n][0]
    return dugum[ilk][2] if dugum[ilk][1]=="TANIMLAYICI" else None
toplam_fn=0; tutan=0; ihlal=[]
for f in dosyalar:
    a = agac(f)
    if a is None:
        print("  ATLA (AST yok):", f); continue
    dugum, cocuk = a
    for i,(d,tur,ad) in enumerate(dugum):
        if tur!="ISLEV": continue
        icerik = altagac(cocuk, i)
        kanal_adlari=set()
        for n in icerik:
            if dugum[n][1]=="DEGISKEN":
                for m in altagac(cocuk,n):
                    if cagri_adi(dugum,cocuk,m)=="kanal_oluştur":
                        kanal_adlari.add(dugum[n][2]); break
        if not kanal_adlari: continue
        # [olcum duzeltmesi] `gonderen(k)` / `alan(k)` PROJEKSIYONLARI da kanal
        # takma adidir. Ilk surum bunlari kacirdi ve kanal_mesaj icin yakalayan=0
        # dedi — oysa D-511 orada kanalin goreve YAKALANDIGINI olcmustu.
        for _tur in range(8):
            once=len(kanal_adlari)
            for n in icerik:
                if dugum[n][1]!="DEGISKEN": continue
                if dugum[n][2] in kanal_adlari: continue
                for m in altagac(cocuk,n):
                    if cagri_adi(dugum,cocuk,m) in ("gönderen","alan"):
                        arg=set(dugum[q][2] for q in altagac(cocuk,m) if dugum[q][1]=="TANIMLAYICI")
                        if kanal_adlari & arg:
                            kanal_adlari.add(dugum[n][2]); break
            if len(kanal_adlari)==once: break
        toplam_fn+=1
        spawn=0; yakalayan=0; join=0
        for n in icerik:
            c=cagri_adi(dugum,cocuk,n)
            if c=="görev_başlat":
                spawn+=1
                gov=set(dugum[m][2] for m in altagac(cocuk,n) if dugum[m][1]=="TANIMLAYICI")
                if kanal_adlari & gov: yakalayan+=1
            elif c=="görev_birleştir": join+=1
        durum = "TUTUYOR" if (yakalayan==0 or join>=yakalayan) else "TUTMUYOR"
        if durum=="TUTUYOR": tutan+=1
        else: ihlal.append((f,ad,yakalayan,join))
        print("  %-44s %-14s kanal=%d spawn=%d yakalayan=%d join=%d  %s" %
              (f.replace("./",""), ad, len(kanal_adlari), spawn, yakalayan, join, durum))
print()
print("=== kanal yaratan islev: %d | TUTUYOR: %d | TUTMUYOR: %d ===" % (toplam_fn, tutan, len(ihlal)))
for f,ad,y,j in ihlal: print("   TUTMUYOR:", f, ad, "yakalayan=%d join=%d"%(y,j))

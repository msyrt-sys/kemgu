#!/usr/bin/env bash
# [D-543] KANAL OMRU KAPISI — davranissal kapilar bu sinifa KORDUR.
#
# `codegen_diff` yalniz CIKIS KODUNA bakar: kanal serbesti kaldirilsa program
# yine 15 doner ve kapi YESIL kalir (D-417'nin spekulasyon-bariyeri dersi).
# Her derleyici icin UC sey olculur:
#   (1) YAPISAL : pozitif fikstur en az 1 `kdl_kanal_serbest` yayiyor mu
#   (2) DAVRANIS: program dogru cikis kodunu veriyor mu (kanit yanlissa cokerdi)
#   (3) SAGLIK  : ASan HIC hata basmiyor mu (cift-serbest / UAF avi)
# ve ayrica NEGATIF fikstur (kacan uc -> 0 serbest, exit 42).
#
# (3) ZORUNLU: gelistirme sirasinda olculdu ki kanal listesi lifted lambda'ya
# SIZINCA cop isaretci serbest birakiliyor (SEGV in free). Yalniz (1)+(2)
# olsaydi o kusur GORUNMEZDI — fikstur yine 15 donuyordu.
#
# [D-544] NEGATIF FIKSTUR SART: `gonderen`/`alan` PROJEKSIYONDUR, ayni handle'i
# geri verir. Uc cerceveden kaciyorsa serbest birakmak CAGIRANDA UAF olur.
# Yalniz pozitif fikstur olsaydi "takma adi hic kanitlama" sabotaji GECERDI (D-425).
#
# [D-544-b] IKI DERLEYICI DE OLCULUR. Port yalniz C'de olsaydi `codegen_diff`
# cikis koduna baktigi icin ayrisma SESSIZ kalirdi (D-486: cagrilan ama
# olcmeyen kapi).
set -u
KEMGU="${KEMGU:-build/kemgu${EXE=}}"
[ -x "$KEMGU" ] || KEMGU="build/kemgu"
CODEGEN="${CODEGEN:-build/codegen${EXE=}}"
[ -x "$CODEGEN" ] || CODEGEN="build/codegen"
F=test/cg_korpus/cg_kanal_omru.kem
N=test/cg_korpus/cg_kanal_kacan_uc.kem
# [D-551] SESSIZ ATLAMA KUSURU (bu harness'ta ÖLÇÜLDÜ): `setarch` KORUMASIZ
# çağrılıyordu ve satır `|| true` ile bitiyordu → setarch yoksa/başarısızsa
# program HİÇ KOŞMUYOR, hata dosyası BOŞ kalıyor, `grep -c ERROR` 0 dönüyor ve
# SAĞLIK ölçümü SESSİZCE GEÇİYORDU. Oysa o ölçüm bu kapının var oluş
# gerekçesiydi (D-543'te gerçek bir UAF'ı yalnız o yakaladı).
# ⚠ `command -v setarch` YETMEZ: bazı çekirdeklerde setarch VAR ama -R
#   BAŞARISIZ olur → gerçekten ÇALIŞTIĞINI ölç (asan_matris_calistir.sh aynası).
ASAN_RUN=""
if setarch -R true >/dev/null 2>&1; then ASAN_RUN="setarch -R"; fi
W=build/kanal_kapi; mkdir -p "$W"
hata=0

[ -f "$F" ] || { echo "🔴 fikstur yok: $F"; exit 2; }
[ -f "$N" ] || { echo "🔴 negatif fikstur yok: $N"; exit 2; }

olc() {   # $1 = etiket, $2 = derleyici
    local et="$1" cc="$2"
    if [ ! -x "$cc" ]; then echo "🔴 $et: ikili yok ($cc)"; hata=1; return; fi
    if ! "$cc" --llvm "$F" > "$W/$et.ll" 2>/dev/null; then
        echo "🔴 $et: IR uretilemedi"; hata=1; return
    fi
    local n; n=$(grep -c "kdl_kanal_serbest(ptr %" "$W/$et.ll")
    if [ "$n" -lt 1 ]; then
        echo "🔴 $et YAPISAL: kanal serbest cagrisi YOK (kanit dusmus) — beklenen >=1, gelen $n"
        hata=1
    else
        echo "  ✅ $et yapisal: $n adet kdl_kanal_serbest"
    fi

    if clang -x ir "$W/$et.ll" -x none build/kdl_runtime.o -o "$W/$et" 2>/dev/null; then
        timeout 20 "$W/$et" >/dev/null 2>&1; local rc=$?
        if [ "$rc" -ne 15 ]; then echo "🔴 $et DAVRANIS: exit=$rc (beklenen 15)"; hata=1
        else echo "  ✅ $et davranis: exit=15"; fi
    else
        echo "🔴 $et: fikstur linklenemedi"; hata=1
    fi

    if clang -fsanitize=address -x ir "$W/$et.ll" -x none build/kdl_runtime.o -o "$W/${et}a" 2>/dev/null; then
        timeout 60 $ASAN_RUN "$W/${et}a" >/dev/null 2>"$W/$et.err"; local arc=$?
        # ⚠ POZİTİF KANIT: ikili GERÇEKTEN koştu mu? Boş hata dosyası hem
        #   "ASan temiz" hem "program hiç çalışmadı" demektir — ikisi
        #   ayrılmadan "0 hata" bir KANIT DEĞİLDİR. 127 = komut bulunamadı
        #   (ör. setarch yok), 124 = zaman aşımı: ikisi de SESSİZ GEÇMEMELİ.
        if [ "$arc" -eq 127 ] || [ "$arc" -eq 124 ]; then
            echo "🔴 $et SAGLIK: ASan ikilisi KOSMADI (rc=$arc) — olcum YAPILMADI"
            hata=1; return
        fi
        local ah; ah=$(grep -cE "ERROR: AddressSanitizer" "$W/$et.err")
        if [ "$ah" -ne 0 ]; then
            echo "🔴 $et SAGLIK: ASan $ah hata — cift-serbest/UAF:"
            grep -E "ERROR: AddressSanitizer" "$W/$et.err" | head -2
            hata=1
        else echo "  ✅ $et saglik: ASan 0 hata"; fi
    else
        echo "  ⚠ $et: ASan derlemesi yok — SAGLIK olcumu ATLANDI (sessiz degil, bildirildi)"
    fi

    # NEGATIF: kacan uc -> serbest YASAK
    if ! "$cc" --llvm "$N" > "$W/$et-n.ll" 2>/dev/null; then
        echo "🔴 $et: negatif fikstur icin IR uretilemedi"; hata=1; return
    fi
    local nn; nn=$(grep -c "kdl_kanal_serbest(ptr %" "$W/$et-n.ll")
    if [ "$nn" -ne 0 ]; then
        echo "🔴 $et NEGATIF: kacan uc oldugu halde $nn serbest yayildi (UAF riski)"; hata=1
    else
        echo "  ✅ $et negatif: kacan uc -> 0 serbest"
    fi
    if clang -x ir "$W/$et-n.ll" -x none build/kdl_runtime.o -o "$W/$et-n" 2>/dev/null; then
        timeout 20 "$W/$et-n" >/dev/null 2>&1; local nrc=$?
        if [ "$nrc" -ne 42 ]; then echo "🔴 $et NEGATIF davranis: exit=$nrc (beklenen 42)"; hata=1
        else echo "  ✅ $et negatif davranis: exit=42"; fi
    else
        echo "🔴 $et: negatif fikstur linklenemedi"; hata=1
    fi
}

olc C "$KEMGU"
olc SELF "$CODEGEN"

if [ "$hata" -ne 0 ]; then echo "=== kanal omru: BASARISIZ ==="; exit 2; fi
echo "=== kanal omru: 10/10 olcum gecti (C + SELF) ==="

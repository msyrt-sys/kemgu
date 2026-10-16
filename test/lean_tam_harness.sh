#!/usr/bin/env bash
# ============================================================================
# lean_tam_harness.sh — [D-529] Lean ispatlari GERCEKTEN DERLENIYOR mu?
# ----------------------------------------------------------------------------
# `calistir_lean_sorry` yalnizca `sorry`/`admit` SAYAR ve kendi ciktisinda
# durustce "⚠ lake build KOSULMADI" der. Ama SORRY'SUZ BIR DOSYA DERLENMIYORSA
# HICBIR SEY KANITLAMAZ — sayim, tip denetiminin yerine gecmez.
#
# NEDEN SIMDIYE KADAR KOSULMUYORDU (olculdu): `lakefile.lean` mathlib4'u
# `require` ediyordu ve lake onu klonlamaya calisip `git exited with code 128`
# ile 11 DAKIKA sonra dusuyordu. D-529'da olculdu ki 32 `.lean` dosyasinin
# HICBIRI Mathlib'i import etmiyor (tum importlar ic: `Kemgu.*`) -> bagimlilik
# bildirim artigiydi. Kaldirilinca proje CEVRIMDISI 45 saniyede derleniyor.
#
# ⚠ `test_tumu`ya BAGLANMADI (bilincli): lean/lake bu depoda WSL'de DEGIL,
#   Windows'ta (~/.elan/bin) kurulu; takim WSL'de kosuyor. Opt-in hedef olarak
#   durur — kuruluysa kosar, degilse ACIKCA bildirip atlar (D-453'un QEMU
#   deseni; D-486'nin yasakladigi sey SESSIZ atlamadir).
# ============================================================================
set -u
PROJE=proofs/drf-v2-lean
[ -d "$PROJE" ] || { echo "🔴 HATA: $PROJE yok — kapı KOŞMADI"; exit 1; }

# elan/lake Windows tarafinda olabilir; PATH'e ekle.
# ⚠ `set -u` altinda `$USER` Git Bash'te TANIMSIZ olabilir -> `${USER:-}`.
for d in "$HOME/.elan/bin" "/c/Users/${USER:-${USERNAME:-}}/.elan/bin"; do
    [ -d "$d" ] && PATH="$d:$PATH"
done
export PATH

# [D-549] WSL → WINDOWS INTEROP. D-529 bu kapıyı "lean/lake Windows'ta, takım
# WSL'de" diye opt-in bırakmıştı; ÖLÇÜLDÜ ve o gerekçe YANLIŞTI — engel ortam
# değil KEŞİFTİ: arama ne `/mnt/c/.../.elan/bin`e ne de `.exe` adına bakıyordu.
# WSL'den `lake.exe` çalışır ve proje dizinindeki `lean-toolchain` (v4.29.0,
# KURULU) çözülür → ÇEVRİMDIŞI derler (ölçüldü: 33 iş, 58 sn).
# ⚠ Dizin DIŞINDA çalıştırmak YANILTIR: orada elan projenin pinini görmez,
#   VARSAYILAN toolchain'i çözüp indirmeye kalkar (ilk ölçümüm tam bu yüzden
#   "ağ gerekiyor" dedi ve YANLIŞTI).
LAKE=""
command -v lake >/dev/null 2>&1 && LAKE=lake
if [ -z "$LAKE" ]; then
    for c in /mnt/c/Users/*/.elan/bin/lake.exe "$HOME/.elan/bin/lake.exe"; do
        [ -x "$c" ] && { LAKE="$c"; break; }
    done
fi

# [D-549] ⚠ WINDOWS lake + WSL DOSYA SİSTEMİ = ÇALIŞMAZ, ve sebebi INCEDIR.
# Interop'ta cwd Windows'a UNC olarak görünür (`\wsl.localhost\...`); `lake
# --version` çalışır (dosya sistemi işi yok) ama `lake build` yalnız
# "error: 1" deyip düşer. Yani proje /mnt/c altında (gerçek bir Windows yolu)
# DEĞİLSE bu kapı koşamaz. ÖLÇÜLDÜ: /mnt/c worktree'sinde 33 iş / 58 sn / rc=0;
# ~/kemgu (WSL fs) kopyasında rc=1.
# Bu, D-529'un "lean/lake Windows'ta, takım WSL'de" gerekçesinin DÜZELTİLMİŞ
# hâlidir: engel lake'in NEREDE KURULU olduğu değil, PROJENİN NEREDE DURDUĞUDUR.
case "$LAKE" in
  *.exe)
    case "$PWD" in
      /mnt/*|[A-Za-z]:*) : ;;   # gerçek Windows yolu — interop çalışır
      *)
        echo "⚠ Windows lake + WSL dosya sistemi — kapı ATLANDI (ortam sınırı)"
        echo "  Sebep: cwd Windows'a UNC görünüyor (\wsl.localhost\...);"
        echo "         lake build UNC cwd'de çalışmaz. --version çalışır, build çalışmaz."
        echo "  Çözüm: bu hedefi /mnt/c altındaki worktree'den koştur."
        exit 0 ;;
    esac ;;
esac

if [ -z "$LAKE" ]; then
    echo "⚠ lake YOK — Lean ispatları derlenemedi, kapı ATLANDI (ortam yeteneği)"
    echo "  Kurulum: elan (https://github.com/leanprover/elan) + lean-toolchain'deki sürüm"
    exit 0
fi

cd "$PROJE" || exit 1
cikti=$("$LAKE" build 2>&1); rc=$?
if [ "$rc" -ne 0 ]; then
    echo "🔴 lake build BAŞARISIZ (rc=$rc):"
    # [D-549] ONCE gercek `error:` satirlari — sabotaj S127'de `tail -20`
    # yalnizca bir LINTER IPUCU gosteriyordu ve kirmizinin SEBEBI gorunmuyordu.
    hata_satir=$(printf %s "$cikti" | grep -E "error:" | head -6)
    if [ -n "$hata_satir" ]; then
        printf %s "$hata_satir" | sed "s/^/     /"
        echo "     ---- (baglam: son 8 satir) ----"
        printf %s "$cikti" | tail -8 | sed "s/^/     /"
    else
        printf '%s\n' "$cikti" | tail -20 | sed 's/^/     /'
    fi
    if printf '%s\n' "$cikti" | grep -q "cloning\|exited with code 128"; then
        echo "     → AĞ/BAĞIMLILIK hatası: bir 'require' dış klon istiyor."
        echo "       D-529: mathlib bildirim artığıydı ve kaldırıldı; yeni bir"
        echo "       bağımlılık eklendiyse GERÇEK bir import'a dayanmalı."
    fi
    exit 1
fi
# ⚠ ARTIMLI derlemede lake HİÇBİR ŞEY basmaz (her şey güncel) → iş sayısı 0
# olur ve bu BAŞARISIZLIK DEĞİLDİR. Ama "0 iş" tek başına yanıltıcıdır: kapı
# gerçekten bir şey derledi mi, yoksa önbellekten mi geçti ayırt edilemez.
# Bu yüzden AYRI bir POZİTİF kanıt aranır: derlenmiş `.olean` çıktıları.
is=$(printf '%s\n' "$cikti" | grep -cE "^✔|Built ")
olean=$(find .lake/build -name "*.olean" 2>/dev/null | wc -l)
if [ "$olean" -eq 0 ]; then
    echo "🔴 lake build OK dedi ama HİÇ .olean ÜRETİLMEMİŞ — kapı boşa koştu"
    exit 1
fi
if [ "$is" -eq 0 ]; then
    echo "=== Lean ispatları: lake build OK (artımlı, güncel; $olean .olean) ==="
else
    echo "=== Lean ispatları derlendi: lake build OK ($is iş, $olean .olean) ==="
fi

#!/usr/bin/env bash
# KEMGU — Lean ispat KAPISI: derleme + AKSİYOM DENETİMİ (D-327)
#
# NEDEN: mevcut `calistir_drf_lean_proof` yalnız `lake build` çalıştırıp
# "sorry/axiom: bkz. README" diyordu — yani sorry-suzluk BELGELENMİŞ ama
# DOĞRULANMAMIŞTI. Bir ispat sisteminde asıl soru "derlendi mi" değil,
# "üst teorem gerçekten ispatlı mı" — Lean bunu `#print axioms` ile söyler:
# çıktıda `sorryAx` varsa ispat DELİKLİDİR (derleme yine de başarılı olur).
#
# Bu kapı:
#   1) tüm modülleri mathlib'SİZ derler (proje hiç Mathlib import etmiyor —
#      ölçüldü; bu yüzden lake/mathlib indirmesi GEREKMEZ, ~1 dk sürer),
#   2) üst teoremlerin aksiyom kümesini yazdırır,
#   3) `sorryAx` görürse KIRMIZI döner.
#
# İzin verilen aksiyomlar Lean'in standart üçlüsüdür: propext,
# Classical.choice, Quot.sound. Bunlar klasik matematiğin temeli, ispat
# deliği DEĞİL. Başka bir şey (özellikle sorryAx) kabul edilmez.
set -u
KOK="proofs/drf-v2-lean"
[ -d "$KOK" ] || { echo "🔴 $KOK yok"; exit 1; }

command -v lean >/dev/null 2>&1 || {
    echo "⏭  lean PATH'te yok — atlandı (elan kurun: ~/.elan/bin PATH'e)"; exit 0; }

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
cd "$KOK" || exit 1

# --- 1) Derleme (bağımlılık sırası: birden çok pas, ilerleme durunca dur) ---
# Kök modül (Kemgu.lean) de derlenir — aksiyom denetimi onu import eder,
# böylece TÜM alt modüller (ör. SideChannel.CT) tek import ile erişilebilir.
dosyalar="$(find Kemgu -name "*.lean" | sed 's|\.lean$||') Kemgu"
toplam=$(echo "$dosyalar" | tr ' ' '\n' | grep -c .)
export LEAN_PATH="$TMP"
for pas in 1 2 3 4 5 6 7 8; do
    ilerleme=0
    for f in $dosyalar; do
        [ -f "$TMP/$f.olean" ] && continue
        mkdir -p "$TMP/$(dirname "$f")"
        if lean --root=. -o "$TMP/$f.olean" "$f.lean" > "$TMP/son_hata.txt" 2>&1; then
            ilerleme=1
        fi
    done
    kurulan=$(find "$TMP" -name "*.olean" | wc -l)
    [ "$kurulan" -eq "$toplam" ] && break
    [ "$ilerleme" -eq 0 ] && break
done
kurulan=$(find "$TMP" -name "*.olean" | wc -l)
if [ "$kurulan" -ne "$toplam" ]; then
    echo "🔴 Lean derleme BAŞARISIZ: $kurulan/$toplam modül"
    head -20 "$TMP/son_hata.txt"
    exit 1
fi
echo "  Lean derleme: $kurulan/$toplam modül ✓"

# --- 2) Aksiyom denetimi ---
# Üst teorem + doğrudan taşıyıcıları. Yeni üst-düzey teorem eklenirse BURAYA da ekle.
cat > "$TMP/aksiyom_denetim.lean" <<'LEANEOF'
import Kemgu
#print axioms Kemgu.Soundness.Main.kemgu_soundness_v3
#print axioms Kemgu.Discharge.NoFault.iyiTipli_no_fault
#print axioms Kemgu.Discharge.NoFault.typed_no_fault
#print axioms Kemgu.MemSafety.Theorems.t1_bellek_guvenligi_tam
#print axioms Kemgu.SideChannel.NonInterference.silme_sim_sVarOku
#print axioms Kemgu.SideChannel.NonInterference.silme_sim_sAtamaTamam
#print axioms Kemgu.SideChannel.NonInterference.silme_sim_cKanalGonderTamam
#print axioms Kemgu.SideChannel.NonInterference.silme_sim_cKanalAlTamam
#print axioms Kemgu.SideChannel.NonInterference.silme_simulasyon
#print axioms Kemgu.SideChannel.NonInterference.ni_cekirdek_altkume
#print axioms Kemgu.SideChannel.NonInterference.izGozlem_izSil
#print axioms Kemgu.SideChannel.CT.ct_ni
#print axioms Kemgu.SideChannel.CT.genel_ifade_korunum
#print axioms Kemgu.SideChannel.CT.ct001_gerekli
#print axioms Kemgu.SideChannel.CTKopru.gomme_sim
#print axioms Kemgu.SideChannel.CTKopru.kopru_ni
#print axioms Kemgu.SideChannel.CTKopru.kopru_bos_degil
#print axioms Kemgu.SideChannel.CT.ct002_gerekli
#print axioms Kemgu.SideChannel.CT.ct004_gerekli
#print axioms Kemgu.SideChannel.CTKopru.kopru_iken_esles_bos_degil
#print axioms Kemgu.SideChannel.CT.ct005_gerekli
#print axioms Kemgu.SideChannel.CTKopru.kopru_indeks_bos_degil
#print axioms Kemgu.SideChannel.CTKopru.storeUyum_ornek
#print axioms Kemgu.SideChannel.CT.ct005y_gerekli
#print axioms Kemgu.SideChannel.CTKopru.kopru_indeks_yaz_bos_degil
#print axioms Kemgu.SideChannel.CT.ct006_gerekli
#print axioms Kemgu.SideChannel.CT.topla_gizli_operand_zararsiz
#print axioms Kemgu.SideChannel.CTKopru.kopru_bol_bos_degil
#print axioms Kemgu.SideChannel.CT.ct006m_gerekli
#print axioms Kemgu.SideChannel.CT.carp_gizli_operand_zararsiz
#print axioms Kemgu.SideChannel.CT.ct_esz_ni
#print axioms Kemgu.SideChannel.CT.esz_zamanlama_etkili
#print axioms Kemgu.SideChannel.CT.esz_capraz_girisim_gercek
#print axioms Kemgu.SideChannel.CT.ct_esz_gerekli
#print axioms Kemgu.SideChannel.CTKopru.kopru_kalan_bos_degil
LEANEOF
if ! lean --root=. "$TMP/aksiyom_denetim.lean" > "$TMP/aks.txt" 2>&1; then
    echo "🔴 aksiyom denetimi çalıştırılamadı:"; head -10 "$TMP/aks.txt"; exit 1
fi
cat "$TMP/aks.txt" | sed 's/^/  /'

if grep -q "sorryAx" "$TMP/aks.txt"; then
    echo "🔴 İSPAT DELİKLİ: aksiyom kümesinde sorryAx var (derleme geçse bile ispat YOK)"
    exit 1
fi
if grep -qE "error" "$TMP/aks.txt"; then
    echo "🔴 aksiyom denetimi hata verdi (teorem adı değişmiş olabilir)"; exit 1
fi
# Kaynakta çıplak `sorry`/`axiom` bildirimi de olmamalı (yorumlar hariç sayılır;
# asıl kanıt yukarıdaki sorryAx denetimidir — bu ikinci savunma hattı).
ham=$(grep -rnE "(^|[[:space:]:=(])sorry([[:space:]]|$|\))" Kemgu --include=*.lean \
      | grep -v "^\S*:[0-9]*: *--" | grep -vc "sorry 0\|sorry YOK\|sorry/axiom")
echo "  ham 'sorry' kalıbı (yorum-dışı): $ham"
echo "=== LEAN İSPAT KAPISI: derleme + aksiyom denetimi GEÇTİ (sorryAx YOK) ==="

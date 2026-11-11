#!/usr/bin/env bash
# [D-751] stdlib MODÜL OLARAK içe alınabiliyor mu? (G8). D-590'dan beri gizlilik her yolda
# zorunlu ve stdlib'in hiçbir bildirimi `genel` değildi → hiçbir modül başka dosyadan
# kullanılamıyordu; kapılar bunu GÖREMİYORDU çünkü stdlib_check modül + testi `cat` ile
# birleştirir (birleşik dosyada gizlilik yoktur). Bu kapı her modülün KENDİ testini modül
# sistemi üzerinden derler: başlık = `kullan stdlib::<yol>::{ <modülün genel adları> };`
# (adlar MODÜL dosyasından okunur), gövde = test dosyası. C ve self-host ikisi de temiz
# geçmeli; tanı listeleri birebir aynı olmalı. kripto/* hariç (birleştirme ile bağlı).
set -u
cd "$(dirname "$0")/.."
KEMGU="${KEMGU:-build/kemgu}"; CODEGEN="${CODEGEN:-build/codegen}"
[ -x "$KEMGU" ] && [ -x "$CODEGEN" ] || { echo "  🔴 derleyici ikilisi yok"; exit 2; }
W="build/_stdlib_ithal_$$"; mkdir -p "$W"; trap 'rm -rf "$W"' EXIT
# MUAFİYET = AÇIK BORÇ ENVANTERİ (LOOP E4m). Satır silinince kapı onu ölçer; muaf modül
# EŞLEŞİRSE kapı uyarır ("artık geçiyor, satırı sil").
#   karsilastir: üyesi `karsilastir` modülün KENDİ adıyla aynı → seçili import T024
#     (modül adı da sembol olarak bağlanıyor). Üye dışlanınca test `karsilastir(3, 5)`
#     çağrısı C'de --check'ten GEÇİP tanımsız @karsilastir'a link-red, self'te T002
#     (C denetleyici modül adını işlev çağrısı olarak kabul ediyor). İkisi de E4m.
#   sonuc: seçili import edilen GENERIC yapı (`KSonuc<T, E>`) C'de tip konumunda T011
#     "bilinmeyen kullanici tipi" (+ ardışık T001); self-host tanıyor. Generic OLMAYAN
#     tipler (json `JsonDeger`) geçiyor. E4m.
MUAF="karsilastir sonuc"
gecti=0; kirmizi=0; muaf=0
for f in stdlib/*.kem stdlib/temel/*.kem; do
  m=$(basename "$f" .kem); [ "$m" = kripto ] && continue
  t="test/stdlib/test_$m.kem"; [ -f "$t" ] || { echo "  🔴 $f: testi yok"; kirmizi=$((kirmizi+1)); continue; }
  yol=$(echo "${f%.kem}" | sed 's#/#::#g')
  adlar=$(grep -oE '^genel (işlev|yapı( tekkez)?|çeşit|sabit) [^ (<:{]+' "$f" | awk '{print $NF}' | paste -sd, | sed 's/,/, /g')
  [ -n "$adlar" ] || { echo "  🔴 $f: hiç genel ad yok"; kirmizi=$((kirmizi+1)); continue; }
  { echo "kullan $yol::{ $adlar };"; cat "$t"; } > "$W/$m.kem"
  "$KEMGU" --checkdump "$W/$m.kem" > "$W/$m.c" 2>&1; rc=$?
  "$CODEGEN" --check "$W/$m.kem" > "$W/$m.s" 2>&1
  em=0; for x in $MUAF; do [ "$x" = "$m" ] && em=1; done
  if [ $em -eq 1 ]; then
    if [ $rc -eq 0 ] && grep -qx "OK" "$W/$m.c" && grep -qx "OK" "$W/$m.s"; then echo "  ⚠ $m MUAF ama artık geçiyor — MUAF satırını sil"; fi
    muaf=$((muaf+1)); continue
  fi
  # --checkdump tanı bassa da 0 döner → "OK" satırı ŞART (iki tarafta da).
  if [ $rc -ne 0 ] || ! grep -qx "OK" "$W/$m.c" || ! grep -qx "OK" "$W/$m.s"; then
    echo "  🔴 $m: modül olarak içe alınamıyor (C rc=$rc)"; head -3 "$W/$m.c" | sed 's/^/       C: /'; head -3 "$W/$m.s" | sed 's/^/       S: /'
    kirmizi=$((kirmizi+1))
  else gecti=$((gecti+1)); fi
done
echo "=== stdlib modül içe alma: $gecti/$((gecti+kirmizi)) modül ($muaf muaf; test modül sistemiyle derlendi) ==="
[ $kirmizi -eq 0 ]

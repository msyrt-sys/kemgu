#!/usr/bin/env bash
# ============================================================================
# dizi_sinir_harness.sh — Dizi sınır-güvenliği KALICI invaryant testi (D-069).
# ----------------------------------------------------------------------------
# Her OOB (sınır-dışı) dizi erişimi PANIC vermeli (temiz durma): rc≠0 + stderr
# "PANIK" içerir. ASLA segfault (rc=139) ve ASLA sessiz-başarı (rc=0+yanlış).
# Geçerli erişim BOZULMAMALI (rc=doğru). Bkz. DECISIONS_LOG D-069.
#
# Kapsam (şimdilik): Kategori 1 (heap dizi_al/yaz OOB) + D-065 koruması.
# Kategori 2 (stack [N×T]) + 3 (güvensiz opt-out) eklendikçe vaka 5/6/8/9 açılır.
#
# Kullanım: bash test/dizi_sinir_harness.sh  (veya make calistir_dizi_sinir_test)
# ============================================================================
set -u
KEMGU=${KEMGU:-build/kemgu.exe}
RT=${RT:-build/kdl_runtime.o}
TMP=$(mktemp -d 2>/dev/null || echo /tmp/dizisinir); mkdir -p "$TMP"
pass=0; fail=0

# panic_bekle <ad> <kaynak>: OOB → PANIC (rc≠0, rc≠139, stderr PANIK) doğrula.
panic_bekle() {
    local ad="$1" src="$2"
    printf '%s\n' "$src" > "$TMP/$ad.kem"
    if ! "$KEMGU" --llvm "$TMP/$ad.kem" > "$TMP/$ad.ll" 2>/dev/null; then
        echo "  🔴 $ad: --llvm üretemedi"; fail=$((fail+1)); return; fi
    if ! clang -x ir "$TMP/$ad.ll" -x none "$RT" -o "$TMP/$ad.exe" 2>/dev/null; then
        echo "  🔴 $ad: link edilemedi"; fail=$((fail+1)); return; fi
    "$TMP/$ad.exe" > "$TMP/$ad.out" 2>"$TMP/$ad.err"; local rc=$?
    if [ "$rc" -eq 139 ]; then echo "  🔴 $ad: SEGFAULT (rc=139) — güvensiz!"; fail=$((fail+1)); return; fi
    if [ "$rc" -eq 0 ]; then echo "  🔴 $ad: sessiz-başarı (rc=0) — OOB yakalanmadı!"; fail=$((fail+1)); return; fi
    if grep -q PANIK "$TMP/$ad.err"; then echo "  ✅ $ad: PANIC (rc=$rc)"; pass=$((pass+1));
    else echo "  🔴 $ad: rc=$rc ama stderr'de PANIK yok"; fail=$((fail+1)); fi
}
# deger_bekle <ad> <beklenen_rc> <kaynak>: geçerli erişim doğru sonuç + rc.
deger_bekle() {
    local ad="$1" brc="$2" src="$3"
    printf '%s\n' "$src" > "$TMP/$ad.kem"
    "$KEMGU" --llvm "$TMP/$ad.kem" > "$TMP/$ad.ll" 2>/dev/null
    clang -x ir "$TMP/$ad.ll" -x none "$RT" -o "$TMP/$ad.exe" 2>/dev/null
    "$TMP/$ad.exe" > "$TMP/$ad.out" 2>"$TMP/$ad.err"; local rc=$?
    if [ "$rc" -eq "$brc" ]; then echo "  ✅ $ad: rc=$rc (doğru)"; pass=$((pass+1));
    else echo "  🔴 $ad: rc=$rc (beklenen $brc) — geçerli erişim bozuldu!"; fail=$((fail+1)); fi
}
# segfault_yok <ad> <dosya>: D-065 koruması — bozuk girdi parse-hatası, segfault DEĞİL.
segfault_yok() {
    local ad="$1" dosya="$2"
    "$KEMGU" --check "$dosya" > "$TMP/$ad.out" 2>"$TMP/$ad.err"; local rc=$?
    if [ "$rc" -eq 139 ]; then echo "  🔴 $ad: SEGFAULT (rc=139)"; fail=$((fail+1));
    else echo "  ✅ $ad: çökme yok (rc=$rc)"; pass=$((pass+1)); fi
}

echo "=== Dizi sınır-güvenliği (D-069) ==="
panic_bekle vaka1_heap_oob_oku 'işlev main() -> tam32 {
    değişken d: Dizi<tam32> = [1, 2, 3];
    ver dizi_al(d, 5);
}'
panic_bekle vaka2_heap_negatif 'işlev main() -> tam32 {
    değişken d: Dizi<tam32> = [1, 2, 3];
    ver dizi_al(d, 0 - 1);
}'
panic_bekle vaka3_heap_oob_yaz 'işlev main() -> tam32 {
    değişken d: Dizi<tam32> = [1, 2, 3];
    dizi_yaz(d, 5, 9);
    ver 0;
}'
panic_bekle vaka4_heap_ptr_oob 'işlev main() -> tam32 {
    değişken d: Dizi<metin> = ["a", "b", "c"];
    değişken s: metin = dizi_al(d, 5);
    ver 0;
}'
panic_bekle vaka5_stack_oob_oku 'işlev main() -> tam32 {
    değişken arr = [1, 2, 3];
    ver arr[5];
}'
panic_bekle vaka6_stack_negatif 'işlev main() -> tam32 {
    değişken arr = [1, 2, 3];
    ver arr[0 - 1];
}'
# D-084: stack [N×T] YAZMA OOB — okuma yolu (vaka5/6) korumalıydı ama yazma
# yolu kontrolsüzdü → `arr[10]=9` SESSİZ buffer-overflow (rc=0). Artık PANIC.
panic_bekle vaka5w_stack_oob_yaz 'işlev main() -> tam32 {
    değişken arr = [1, 2, 3];
    arr[5] = 9;
    ver 0;
}'
panic_bekle vaka6w_stack_negatif_yaz 'işlev main() -> tam32 {
    değişken arr = [1, 2, 3];
    arr[0 - 1] = 9;
    ver 0;
}'
deger_bekle vaka7w_stack_yaz_gecerli 9 'işlev main() -> tam32 {
    değişken arr = [1, 2, 3];
    arr[1] = 9;
    ver arr[1];
}'
deger_bekle vaka7_heap_gecerli 60 'işlev main() -> tam32 {
    değişken d: Dizi<tam32> = [10, 20, 30];
    ver dizi_al(d, 0) + dizi_al(d, 1) + dizi_al(d, 2);
}'
deger_bekle vaka7b_stack_gecerli 60 'işlev main() -> tam32 {
    değişken arr = [10, 20, 30];
    ver arr[0] + arr[1] + arr[2];
}'
# D-085: TÜRETİLMİŞ heap dizi tabanı `[]` (yapı-alanı / çağrı-dönüşü) — eskiden
# KdlDizi* descriptor'ını düz veri sanıp GEP yapıyordu (sessiz-yanlış / segfault).
deger_bekle vaka11_erisim_oku 42 'yapı Kap { xs: Dizi<tam32>; }
işlev main() -> tam32 {
    değişken k: Kap = Kap { xs: [10, 20, 12] };
    ver k.xs[0] + k.xs[1] + k.xs[2];
}'
deger_bekle vaka12_erisim_yaz 27 'yapı Kap { xs: Dizi<tam32>; }
işlev main() -> tam32 {
    değişken k: Kap = Kap { xs: [10, 20, 12] };
    k.xs[1] = 5;
    ver k.xs[0] + k.xs[1] + k.xs[2];
}'
deger_bekle vaka13_cagri_oku 42 'işlev yap() -> Dizi<tam32> { ver [10, 20, 12]; }
işlev main() -> tam32 {
    değişken xs: Dizi<tam32> = yap();
    ver xs[0] + xs[1] + xs[2];
}'
deger_bekle vaka14_direct_heap_yaz 99 'işlev main() -> tam32 {
    değişken xs: Dizi<tam32> = [1, 2, 3];
    xs[1] = 99;
    ver xs[1];
}'
# Türetilmiş heap dizi OOB de runtime sınır-kontrollü (kdl_dizi_al/yaz PANIC).
panic_bekle vaka15_erisim_oob 'yapı Kap { xs: Dizi<tam32>; }
işlev main() -> tam32 {
    değişken k: Kap = Kap { xs: [1, 2, 3] };
    ver k.xs[9];
}'
# D-086: &Dizi<T> referans param — girişte deref → normal heap dizi. dizi_al/
# dizi_boyut/[] tutarlı (eskiden çift-pointer çöp/PANIK; dizi_boyut(&Dizi) T001).
deger_bekle vaka16_ref_dizi_al 7 'işlev oku(xs: &Dizi<tam32>) -> tam32 { ver dizi_al(xs, 0); }
işlev main() -> tam32 {
    değişken a: Dizi<tam32> = [7, 8, 9];
    ver oku(&a);
}'
deger_bekle vaka17_ref_dizi_boyut 3 'işlev say(xs: &Dizi<tam32>) -> tam32 { ver dizi_boyut(xs); }
işlev main() -> tam32 {
    değişken a: Dizi<tam32> = [7, 8, 9];
    ver say(&a);
}'
deger_bekle vaka18_ref_indeks 8 'işlev oku(xs: &Dizi<tam32>) -> tam32 { ver xs[1]; }
işlev main() -> tam32 {
    değişken a: Dizi<tam32> = [7, 8, 9];
    ver oku(&a);
}'
# &Dizi mutasyonu çağırana yansır (paylaşılan descriptor).
deger_bekle vaka19_ref_mutasyon 99 'işlev yaz(xs: &değişken Dizi<tam32>) -> tam32 { dizi_yaz(xs, 0, 99); ver 0; }
işlev main() -> tam32 {
    değişken a: Dizi<tam32> = [7, 8, 9];
    değişken r: tam32 = yaz(&değişken a);
    ver dizi_al(a, 0);
}'
# D-087: by-value YAPI elemanlı dizi (Dizi<Yapı>). Eskiden skaler i32 getter +
# eleman_byte=4 (truncation/link-fail); artık kdl_dizi_*_yapi memcpy + sizeof.
deger_bekle vaka20_struct_indeks_oku 42 'yapı Nokta { x: tam32; y: tam32; }
işlev main() -> tam32 {
    değişken ps: Dizi<Nokta> = [Nokta { x: 40, y: 2 }];
    ver ps[0].x + ps[0].y;
}'
deger_bekle vaka21_struct_dizi_al 42 'yapı Nokta { x: tam32; y: tam32; }
işlev main() -> tam32 {
    değişken ps: Dizi<Nokta> = [Nokta { x: 40, y: 2 }];
    değişken p: Nokta = dizi_al(ps, 0);
    ver p.x + p.y;
}'
deger_bekle vaka22_struct_yaz 50 'yapı Nokta { x: tam32; y: tam32; }
işlev main() -> tam32 {
    değişken ps: Dizi<Nokta> = [Nokta { x: 1, y: 2 }];
    ps[0] = Nokta { x: 20, y: 30 };
    değişken p: Nokta = dizi_al(ps, 0);
    ver p.x + p.y;
}'
# Padding doğruluğu: {tam8, tam64} struct → sizeof const-expr LLVM layout'uyla birebir.
deger_bekle vaka23_struct_padding 42 'yapı M { a: tam8; b: tam64; }
işlev main() -> tam32 {
    değişken xs: Dizi<M> = [M { a: 2, b: 40 }];
    değişken m: M = dizi_al(xs, 0);
    ver m.b + m.a;
}'
# Struct dizi OOB de runtime sınır-kontrollü.
panic_bekle vaka24_struct_oob 'yapı Nokta { x: tam32; y: tam32; }
işlev main() -> tam32 {
    değişken ps: Dizi<Nokta> = [Nokta { x: 1, y: 2 }];
    değişken p: Nokta = dizi_al(ps, 9);
    ver p.x;
}'
# vaka8: güvensiz opt-out — stack indeks güvensiz blokta sınır-kontrolsüz (panic IR yok).
opt_out_kontrol() {
    local ad="vaka8_guvensiz_optout"
    printf '%s\n' 'işlev main() -> tam32 {
    değişken arr = [10, 20, 30];
    değişken r: tam32 = 0;
    güvensiz { r = arr[1]; }
    ver r;
}' > "$TMP/$ad.kem"
    "$KEMGU" --llvm "$TMP/$ad.kem" > "$TMP/$ad.ll" 2>/dev/null
    local pc; pc=$(grep -c 'call void @kdl_panik' "$TMP/$ad.ll")
    clang -x ir "$TMP/$ad.ll" -x none "$RT" -o "$TMP/$ad.exe" 2>/dev/null
    "$TMP/$ad.exe" >/dev/null 2>&1; local rc=$?
    if [ "$pc" -eq 0 ] && [ "$rc" -eq 20 ]; then
        echo "  ✅ $ad: sınır-kontrol IR yok + rc=20 (opt-out + geçerli)"; pass=$((pass+1));
    else echo "  🔴 $ad: panik-IR=$pc rc=$rc (0 + 20 bekle)"; fail=$((fail+1)); fi
}
opt_out_kontrol
# vaka9: stack-array DEĞİŞKENİ Dizi<T> param'a → checker G003 reddi (çökmeden ÖNCE).
g003_reddi() {
    local ad="vaka9_g003_reddi"
    printf '%s\n' 'işlev topla(xs: Dizi<tam32>, n: tam32) -> tam32 { ver dizi_al(xs, 0); }
işlev main() -> tam32 {
    değişken xs = [10, 20, 12];
    ver topla(xs, 3);
}' > "$TMP/$ad.kem"
    "$KEMGU" --checkdump "$TMP/$ad.kem" 2>/dev/null > "$TMP/$ad.out"
    if grep -q '^G003' "$TMP/$ad.out"; then
        echo "  ✅ $ad: G003 reddi (compile-time, çökme yok)"; pass=$((pass+1));
    else echo "  🔴 $ad: G003 bekleniyordu, çıktı: $(head -1 "$TMP/$ad.out")"; fail=$((fail+1)); fi
}
g003_reddi
segfault_yok vaka10_d065_koruma test/lex_korpus/m3_04_ayrac_hata.kem

echo "=== dizi sınır-güvenliği: $pass/$((pass+fail)) ==="
[ "$fail" -eq 0 ]

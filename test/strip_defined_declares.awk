# K1/K2/K3 (D-260/262/261): SAF-.kem runtime IR'ından ÇAKIŞAN declare'ları sil.
# kemgu --llvm boilerplate `declare @f(...)` emit eder; kem_heap.kem AYNI f'i
# `define` ederse LLVM "invalid redefinition" verir. Bu awk: bir fonksiyon hem
# define hem declare ediliyorsa declare'ı DÜŞÜR (define kalır → tek tanım). Robust:
# yeni .kem-runtime fn eklendikçe strip listesini elle güncellemeye gerek yok.
/^define /{ if (match($0, /@[a-zA-Z_][a-zA-Z0-9_]*/)) def[substr($0, RSTART, RLENGTH)] = 1 }
{ lines[NR] = $0 }
END {
    for (i = 1; i <= NR; i++) {
        l = lines[i]
        if (l ~ /^declare /) {
            if (match(l, /@[a-zA-Z_][a-zA-Z0-9_]*/)) {
                nm = substr(l, RSTART, RLENGTH)
                if (nm in def) continue    # define edilen fn'in declare'ını atla
            }
        }
        print l
    }
}

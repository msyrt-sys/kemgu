#!/bin/bash
# [D-772] Ölçümlü koşum kabuğu. `make test_tumu_olcum` bunu SHELL yapar; make her tarif satırını
# `zaman_kabuk.sh <hedef> -c '<satır>'` olarak çağırır. Satır bash'te AYNEN koşar (çıkış kodu
# korunur); başlangıç/bitiş ve çıkış kodu KEMGU_ZAMAN_DOSYASI'na eklenir.
# $(shell …) ve hedefsiz çağrılarda $@ boştur → ilk argüman doğrudan -c olur: ölçmeden koş.
if [ "$1" = "-c" ] || [ -z "${KEMGU_ZAMAN_DOSYASI:-}" ]; then
    [ "$1" = "-c" ] || shift
    exec /bin/bash "$@"
fi
hedef=$1; shift
s=$(date +%s.%N)
/bin/bash "$@"; rc=$?
son=$(date +%s.%N)
if command -v flock >/dev/null 2>&1; then
    # Bütün zaman satırı tek kilit altında yazılır, tarif çıkışı ayrıca korunur.
    {
        flock -x 9 || exit 1
        printf '%s\t%s\t%s\t%d\n' "$hedef" "$s" "$son" "$rc" >> "$KEMGU_ZAMAN_DOSYASI" || exit 1
    } 9>> "$KEMGU_ZAMAN_DOSYASI.kilit"
else
    python3 tools/kapi/kilitli.py "$KEMGU_ZAMAN_DOSYASI.kilit" bash -c \
        'printf "%s\t%s\t%s\t%d\n" "$@" >> "$KEMGU_ZAMAN_DOSYASI"' \
        bash "$hedef" "$s" "$son" "$rc" || exit 1
fi
exit "$rc"

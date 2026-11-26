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
printf '%s\t%s\t%s\t%d\n' "$hedef" "$s" "$(date +%s.%N)" "$rc" >> "$KEMGU_ZAMAN_DOSYASI"
exit $rc

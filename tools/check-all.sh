#!/usr/bin/env bash
# Come Back Window — tüm denetimleri sırayla çalıştırır.
#
# Kullanım: tools/check-all.sh [--quick]
#   --quick : entegrasyon testini atlar (KWin oturumu gerekmez)
#
# Her adım PASS/FAIL/SKIP olarak raporlanır; en az bir adım başarısız olursa
# betik 1 ile çıkar. Kurulu olmayan araçlar için adımlar SKIP döner.
set -u

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
QUICK=0
FAILED=0

for arg in "$@"; do
    case "$arg" in
    --quick) QUICK=1 ;;
    -h | --help)
        sed -n '2,9p' "$0"
        exit 0
        ;;
    *)
        printf 'Bilinmeyen seçenek: %s\n' "$arg" >&2
        exit 2
        ;;
    esac
done

cd "$ROOT" || exit 1

run_step() {
    name=$1
    shift
    printf '\n== %s ==\n' "$name"
    output=$("$@" 2>&1)
    status=$?
    printf '%s\n' "$output"
    last=$(printf '%s\n' "$output" | grep -v '^[[:space:]]*$' | tail -n 1)
    if [ "$status" -ne 0 ]; then
        printf 'FAIL %s\n' "$name"
        FAILED=1
    elif printf '%s' "$last" | grep -q '^SKIP'; then
        printf 'SKIP %s\n' "$name"
    else
        printf 'PASS %s\n' "$name"
    fi
}

run_step "derleme" make -j"$(nproc 2>/dev/null || echo 2)"
run_step "make check (lint + birim + JS + doğrulama)" make check
run_step "ASan/UBSan birim testleri" make test-asan
run_step "valgrind birim testleri" make test-valgrind

if [ "$QUICK" -eq 0 ]; then
    run_step "entegrasyon testleri" make test-integration
fi

printf '\n'
if [ "$FAILED" -ne 0 ]; then
    printf 'check-all: BAZI ADIMLAR BAŞARISIZ\n'
    exit 1
fi
printf 'check-all: TÜMÜ GEÇTİ\n'

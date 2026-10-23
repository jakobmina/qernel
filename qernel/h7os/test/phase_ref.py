#!/usr/bin/env python3
"""
phase_ref.py — la fase áurea exacta de src/h7.c contra precisión arbitraria.

  python3 test/phase_ref.py build/phase_tool [build/boot.log]

Con decimal a 120 dígitos (sin el código C):
  1. h7_label(n) = ⌊frac(n·φ)·2^64⌋, con a lo más 1 de diferencia por el
     truncado, para casos borde y 4000 enteros de 64 bits de ambos signos;
  2. h7_golden(n) = cos(πφn) a menos de 1e-15;
  3. informa cuánto se aleja rotate_pow (la versión de v0.6) en esos mismos n;
  4. si se da boot.log, comprueba las líneas `phase` del kernel.
Sale con 0 si todo coincide.
"""
import math
import re
import subprocess
import sys
from decimal import Decimal, getcontext

getcontext().prec = 120
PHI = (1 + Decimal(5).sqrt()) / 2
fail = 0


def report(ok, text):
    global fail
    print(('ok    ' if ok else 'FALLO ') + text)
    fail |= not ok


def frac(x):
    return x - math.floor(x)                                # Decimal exacto, también para x < 0


def exact(n):
    lab = int(frac(n * PHI) * (Decimal(2) ** 64))          # ⌊frac(nφ)·2^64⌋
    x = float(frac(n * PHI / 2))                            # cos(πφn) = cos(2π·frac(nφ/2))
    if x >= 0.5:
        x -= 1.0
    return lab, math.cos(2 * math.pi * x)


def main():
    tool = sys.argv[1]
    rows = subprocess.run([tool], check=True, capture_output=True, text=True).stdout.split('\n')
    worst_lab = worst_cos = 0
    rp_err = {}
    count = 0
    for row in filter(None, rows):
        n, lab, gold, rp = row.split()
        n = int(n)
        e_lab, e_cos = exact(n)
        d = (int(lab, 16) - e_lab) % 2 ** 64
        worst_lab = max(worst_lab, min(d, 2 ** 64 - d))
        worst_cos = max(worst_cos, abs(float(gold) - e_cos))
        mag = len(str(abs(n))) if n else 1
        rp_err[mag] = max(rp_err.get(mag, 0.0), abs(float(rp) - e_cos))
        count += 1
    report(worst_lab <= 1, f'h7_label = ⌊frac(nφ)·2^64⌋ en {count} enteros de 64 bits '
                           f'(máx diferencia {worst_lab} en la última unidad)')
    report(worst_cos <= 1e-15, f'h7_golden = cos(πφn) en los mismos n (máx |Δ| = {worst_cos:.1e})')
    print('info  rotate_pow (v0.6) en esos n: ' + ' · '.join(
        f'{d} dígitos {rp_err[d]:.0e}' for d in (4, 7, 10, 13, 16, 19) if d in rp_err))

    if len(sys.argv) > 2:
        log = open(sys.argv[2], encoding='utf-8').read()
        hits = re.findall(r'^n=(-?\d+) · nodo .*\nfrac\(nφ\)\s+(\S+) · 0x([0-9A-F]{16})\nO_n exacta\s+(\S+)', log, re.M)
        ok = bool(hits)
        for n, fr, lab, gold in hits:
            e_lab, e_cos = exact(int(n))
            ok &= abs(int(lab, 16) - e_lab) <= 1 and abs(float(gold) - e_cos) <= 6e-10 \
                and abs(float(fr) - e_lab / 2 ** 64) <= 6e-10
        report(ok, f'phase en el kernel = referencia ({len(hits)} comandos)')
    sys.exit(fail)


if __name__ == '__main__':
    main()

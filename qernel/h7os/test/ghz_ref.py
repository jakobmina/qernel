#!/usr/bin/env python3
"""
ghz_ref.py — verificación independiente de `q ghz` (src/ghz.c).

  python3 test/ghz_ref.py build/ghz_exp

Sin el código C del simulador, para n = −49..62:
  1. numpy corre el QASM de codificación y el estado es (|r> + (−1)^n |7−r>)/√2;
  2. numpy corre el QASM con lectura: un solo resultado con P = 1, y ese
     resultado nombra el estado GHZ (|0x> ± |1x̄>)/√2 que se codificó
     (solapamiento 1 con el estado del paso 1);
  3. la lectura más el bit r ≥ 4 devuelve n mod 14;
  4. `ghz_exp raw` (el simulador del kernel) da la misma lectura y P.
Sale con 0 si todo coincide.
"""
import os
import subprocess
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(__file__))
from qasm_ref import Sim  # noqa: E402

fail = 0
A, B = -49, 62


def report(ok, text):
    global fail
    print(('ok    ' if ok else 'FALLO ') + text)
    fail |= not ok


def qasm(tool, n, enc):
    args = [tool, 'qasm', str(n)] + (['enc'] if enc else [])
    return subprocess.run(args, check=True, capture_output=True, text=True).stdout


def ghz_basis(code):
    """(|0 x> + (−1)^s |1 x̄>)/√2 con s = bit alto de code y x = sus dos bits bajos"""
    s, x = code >> 2, code & 3
    v = np.zeros(8, complex)
    v[x] = 1
    v[4 | (3 - x)] = (-1) ** s
    return v / np.sqrt(2)


def main():
    tool = sys.argv[1]
    raw = {}
    for line in subprocess.run([tool, 'raw', str(A), str(B)], check=True, capture_output=True, text=True).stdout.split('\n'):
        if line:
            n, code, p, ok = line.split()
            raw[int(n)] = (int(code), float(p), int(ok))
    st_ok = det_ok = back_ok = same_ok = 0
    total = B - A + 1
    for n in range(A, B + 1):
        r = n % 7
        want = np.zeros(8, complex)
        want[r] = 1
        want[7 - r] += (-1) ** (n % 2)
        want /= np.linalg.norm(want)
        psi = Sim(qasm(tool, n, True), stop_at_measure=True).run().psi
        st_ok += abs(abs(np.vdot(want, psi)) - 1) < 1e-12
        probs = np.abs(Sim(qasm(tool, n, False), stop_at_measure=True).run().psi) ** 2
        code = int(np.argmax(probs))
        det_ok += probs[code] > 1 - 1e-12 and abs(abs(np.vdot(ghz_basis(code), psi)) - 1) < 1e-12
        rr = 7 - (code & 3) if r >= 4 else (code & 3)                  # el bit r ≥ 4 elige el miembro
        k = next(k for k in range(14) if k % 7 == rr and k % 2 == code >> 2)
        back_ok += k == n % 14
        same_ok += raw[n][0] == code and abs(raw[n][1] - probs[code]) < 1e-12 and raw[n][2] == 1
    report(st_ok == total, f'numpy: el estado codificado es (|r> + (−1)^n |7−r>)/√2 en {st_ok}/{total} (n = {A}..{B})')
    report(det_ok == total, f'numpy: lectura con P = 1 que nombra el estado GHZ codificado en {det_ok}/{total}')
    report(back_ok == total, f'lectura + bit r ≥ 4 = n mod 14 en {back_ok}/{total}')
    report(same_ok == total, f'simulador del kernel = numpy en lectura, P y estado en {same_ok}/{total}')
    sys.exit(fail)


if __name__ == '__main__':
    main()

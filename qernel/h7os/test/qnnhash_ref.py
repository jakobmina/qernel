#!/usr/bin/env python3
"""
qnnhash_ref.py — verificación independiente de qnnhash.c: la covarianza y la
asimetría del hash QNN de Jako (h7_qnn_hash.py), calculadas exactas (Born,
sin Aer ni *shots*) en vez de muestreadas.

  python3 test/qnnhash_ref.py build/qnnhash_tool

No usa Qiskit: aplica a mano, con numpy, las mismas matrices estándar que
Qiskit define para H, RZ, RY, RX, CCX y CSWAP (son definiciones de libro de
texto, no algo propio de Qiskit), sobre un vector de 8 amplitudes. El bit q
del índice es el qubit q, igual que en qsv.c.

n_z7, o_n = cos(πn) e i_n = cos(πφn) se recalculan aquí con math.cos/Python
puro, independiente de rotate_pow (Cody-Waite + Taylor) de h7.c — así esta
referencia también vigila que rotate_pow no se haya desviado.
"""
import math
import subprocess
import sys

import numpy as np

PHI = (1 + math.sqrt(5)) / 2
I2 = np.eye(2, dtype=complex)
H2 = np.array([[1, 1], [1, -1]], dtype=complex) / np.sqrt(2)


def rz(l):
    return np.diag([np.exp(-1j * l / 2), np.exp(1j * l / 2)])


def ry(t):
    c, s = math.cos(t / 2), math.sin(t / 2)
    return np.array([[c, -s], [s, c]], dtype=complex)


def rx(t):
    c, s = math.cos(t / 2), math.sin(t / 2)
    return np.array([[c, -1j * s], [-1j * s, c]], dtype=complex)


def apply1(psi, n, q, U):
    """aplica U al qubit q de un estado de n qubits (bit q del índice = qubit q)"""
    mats = [I2] * n
    mats[q] = U
    op = mats[n - 1]
    for k in range(n - 2, -1, -1):
        op = np.kron(op, mats[k])
    return op @ psi


def ccx_apply(psi, c1, c2, t):
    out = psi.copy()
    for i in range(len(psi)):
        if (i >> c1) & 1 and (i >> c2) & 1 and not (i >> t) & 1:
            j = i | (1 << t)
            out[i], out[j] = psi[j], psi[i]
    return out


def cswap_apply(psi, c, t1, t2):
    out = psi.copy()
    for i in range(len(psi)):
        if (i >> c) & 1:
            b1, b2 = (i >> t1) & 1, (i >> t2) & 1
            if b1 != b2:
                j = i ^ (1 << t1) ^ (1 << t2)
                out[i] = psi[j]
    return out


def n_z7_py(n):
    m = n % 7
    return 7 if (m == 0 and n != 0) else m


def qnn_step_ref(n):
    nz7 = n_z7_py(n)
    c_nz7 = 7 - nz7
    o_n = math.cos(math.pi * n)
    i_n = math.cos(math.pi * PHI * n)
    pcv = o_n * i_n + i_n
    d_s = float(n + c_nz7 - nz7)

    psi = np.zeros(8, dtype=complex)
    psi[0] = 1
    for q in range(3):
        psi = apply1(psi, 3, q, H2)
    psi = apply1(psi, 3, 0, rz(d_s))
    psi = apply1(psi, 3, 0, ry(d_s))
    psi = apply1(psi, 3, 0, rx(i_n))
    psi = cswap_apply(psi, 0, 2, 1)
    psi = ccx_apply(psi, 2, 1, 0)
    psi = ccx_apply(psi, 1, 0, 2)

    probs = np.abs(psi) ** 2
    Eq1 = sum(probs[i] for i in range(8) if (i >> 1) & 1)
    Eq2 = sum(probs[i] for i in range(8) if (i >> 2) & 1)
    Eq1q2 = sum(probs[i] for i in range(8) if ((i >> 1) & 1) and ((i >> 2) & 1))
    cov = Eq1q2 - Eq1 * Eq2
    asym = Eq2 - Eq1
    dist_alt = math.sqrt(cov ** 2 + asym ** 2)
    return dict(n_z7=nz7, c_n_z7=c_nz7, o_n=o_n, i_n=i_n, pcv=pcv, cov=cov, asym=asym, dist_alt=dist_alt)


def qnn_hash_ref(seed, iterations=3):
    current = seed
    steps = []
    for _ in range(iterations):
        st = qnn_step_ref(current)
        learning_rate = 1000
        delta_f = (st['dist_alt'] * learning_rate * PHI) if st['dist_alt'] > 1e-9 \
            else (st['cov'] * learning_rate * PHI)
        delta_n = int(delta_f)                     # trunca hacia 0, como Python real
        next_n = abs(current + delta_n + st['c_n_z7'])
        st['delta_n'], st['next_n'] = delta_n, next_n
        steps.append(st)
        current = next_n
    final = qnn_step_ref(current)
    return dict(steps=steps, final_n=current, psi=final['i_n'], energy=final['pcv'], torsion=2 * math.pi - 7)


def parse_tool_raw(line, iterations):
    f = line.split()
    seed, n_iter = int(f[0]), int(f[1])
    steps = []
    p = 2
    for _ in range(n_iter):
        nz7, c_nz7 = int(f[p]), int(f[p + 1])
        o_n, i_n, pcv, cov, asym, dist_alt = map(float, f[p + 2:p + 8])
        delta_n, next_n = int(f[p + 8]), int(f[p + 9])
        steps.append(dict(n_z7=nz7, c_n_z7=c_nz7, o_n=o_n, i_n=i_n, pcv=pcv,
                           cov=cov, asym=asym, dist_alt=dist_alt, delta_n=delta_n, next_n=next_n))
        p += 10
    final_n = int(f[p])
    psi, energy, torsion = map(float, f[p + 1:p + 4])
    return dict(seed=seed, steps=steps, final_n=final_n, psi=psi, energy=energy, torsion=torsion)


def main():
    tool = sys.argv[1] if len(sys.argv) > 1 else 'build/qnnhash_tool'
    fail = 0
    worst = 0.0
    seeds = [0, 1, 2, 3, 6, 7, 8, 12, 14, 49, 100, 271, -5, -7, -49, 12345, 999999]
    for seed in seeds:
        out = subprocess.run([tool, 'raw', str(seed)], check=True, capture_output=True, text=True).stdout
        tool_out = parse_tool_raw(out, 3)
        ref = qnn_hash_ref(seed)

        ok_int = tool_out['final_n'] == ref['final_n']
        for i, (ts, rs) in enumerate(zip(tool_out['steps'], ref['steps'])):
            if ts['delta_n'] != rs['delta_n'] or ts['next_n'] != rs['next_n']:
                ok_int = False
                print(f"  seed={seed} iter={i + 1}: delta_n/next_n no coinciden "
                      f"(tool {ts['delta_n']},{ts['next_n']} vs ref {rs['delta_n']},{rs['next_n']}; "
                      f"dist_alt tool={ts['dist_alt']:.15f} ref={rs['dist_alt']:.15f})")
            d = max(abs(ts['cov'] - rs['cov']), abs(ts['asym'] - rs['asym']), abs(ts['i_n'] - rs['i_n']))
            worst = max(worst, d)

        d_final = max(abs(tool_out['psi'] - ref['psi']), abs(tool_out['energy'] - ref['energy']),
                       abs(tool_out['torsion'] - ref['torsion']))
        worst = max(worst, d_final)
        status = 'ok   ' if (ok_int and d_final < 1e-9) else 'FALLO'
        if not (ok_int and d_final < 1e-9):
            fail = 1
        print(f"{status} seed={seed:>8}  n_final tool={tool_out['final_n']:<6} ref={ref['final_n']:<6}  "
              f"psi/energy/torsion |Δ|={d_final:.2e}")

    print(f"\npeor |Δ| en covarianza/asimetria/i_n/psi/energy/torsion sobre todas las semillas: {worst:.2e}")
    print("ok   " if not fail else "FALLO", "- comparado contra H, RZ, RY, RX, CCX, CSWAP aplicados a mano con numpy")
    sys.exit(fail)


if __name__ == '__main__':
    main()

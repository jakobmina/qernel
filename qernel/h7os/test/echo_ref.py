#!/usr/bin/env python3
"""
echo_ref.py — verificación independiente del experimento del eco.

  python3 test/echo_ref.py build/echo_exp

Hace tres cosas, ninguna con el código C del simulador:
  1. formas cerradas: recalcula las predicciones del prerregistro;
  2. los mismos circuitos: corre en numpy (qasm_ref.Sim) todos los QASM que
     exporta echo_exp y reconstruye r; debe coincidir con `echo_exp raw` (±1e-9);
  3. otra Monte Carlo: implementa los siete modelos directamente (fases o
     producto tensorial con el entorno), con el generador de numpy y 20000
     realizaciones, y la compara con la forma cerrada.
Sale con 0 si todo coincide.
"""
import os
import re
import subprocess
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(__file__))
from qasm_ref import Sim  # noqa: E402

K, DT = 8, 1 / 8
MODELS = 'ABCDEFG'
fail = 0


def report(ok, text):
    global fail
    print(('ok    ' if ok else 'FALLO ') + text)
    fail |= not ok


# --- 1. formas cerradas ------------------------------------------------------

def telegraph(a, q, eco):
    """E[e^{iθ}] de un qubit: telégrafo ±a con cambio de signo q por paso"""
    v = np.array([0.5, 0.5], complex)
    P = np.array([[1 - q, q], [q, 1 - q]])
    for k in range(K):
        sgn = -1 if eco and k >= K // 2 else 1
        v = v * np.exp(1j * sgn * a * DT * np.array([1, -1]))
        if k < K - 1:
            v = v @ P
    return abs(v.sum()) ** 3


def closed_forms():
    a = (np.sin(3) / 3) ** 3
    b = (1 - 2 * 0.02) ** 24
    e = np.cos(3 * DT) ** 24
    g = 1 / (2 * np.sqrt(2))
    return {'A': (a, 1.0), 'B': (b, b), 'C': (a * b, b), 'D': (telegraph(2, 0.05, 0), telegraph(2, 0.05, 1)),
            'E': (e, e), 'F': (0.0, 1.0), 'G': (g, g)}


# lo que dice la tabla del prerregistro, a 4 decimales («≈ 0» se escribe 0)
PREREG = {'A': (0.0001, 1.0), 'B': (0.3754, 0.3754), 'C': (0.0000, 0.3754), 'D': (0.0017, 0.5606),
          'E': (0.1775, 0.1775), 'F': (0.0, 1.0), 'G': (0.3536, 0.3536)}


# --- 2. los mismos circuitos en numpy -------------------------------------

def stats(c):
    """r, σ (método delta) y p = exp(-M r²) de las c_i, como en echo_stat (v0.7)"""
    mu = np.mean(c)
    r = abs(mu)
    if len(c) == 1:
        return r, None, None
    X = np.c_[c.real, c.imag]
    cov = np.cov(X.T, bias=True)
    u = np.array([mu.real, mu.imag]) / r if r > 0 else None
    var = u @ cov @ u if r > 0 else np.trace(cov) / 2
    return r, np.sqrt(max(var, 0) / len(c)), np.exp(-len(c) * r * r)


def same_circuits(tool):
    out, st = {}, {}
    for m in MODELS:
        rs = []
        for eco in ('libre', 'eco'):
            c = 0
            for basis in ('xxx', 'yxx'):
                text = subprocess.run([tool, 'dump', m, eco, basis], check=True, capture_output=True, text=True).stdout
                circuits = [x for x in re.split(r'(?=^OPENQASM)', text, flags=re.M) if x.strip()]
                vals = np.array([Sim(t, stop_at_measure=True).run().parity('q') for t in circuits])
                c = c + (vals if basis == 'xxx' else 1j * vals)
            rs.append(abs(np.mean(c)))
            st[m, eco] = stats(np.atleast_1d(c))
        out[m] = tuple(rs)
    return out, st


def check_stats(tool, st):
    """el bloque «después del prerregistro» de echo_exp = σ y p recalculados en numpy"""
    text = subprocess.run([tool], check=True, capture_output=True, text=True).stdout
    cell = r'(\d\.\d{4}) (?:± (\d\.\d{4})\s+(< 0\.001|\d\.\d{3})|\(exacto\))'
    rows = re.findall(rf'^([A-G])  {cell}\s+{cell}$', text, re.M)
    ok = len(rows) == len(MODELS)
    for m, *f in rows:
        for k, eco in enumerate(('libre', 'eco')):
            r, sd, p = f[3 * k:3 * k + 3]
            nr, nsd, np_ = st[m, eco]
            ok &= abs(float(r) - nr) <= 6e-5
            if nsd is None:
                ok &= sd == ''
            else:
                ok &= abs(float(sd) - nsd) <= 6e-5
                ok &= (np_ < 0.001) if p == '< 0.001' else abs(float(p) - np_) <= 6e-4
    return ok, len(rows)


# --- 3. Monte Carlo propia ----------------------------------------------------

def own_monte_carlo(M=20000, seed=2026):
    rng = np.random.default_rng(seed)
    sgn = np.r_[np.ones(K // 2), -np.ones(K // 2)][None, :, None]     # el eco invierte la 2.ª mitad
    out = {}
    for m in 'ABCDE':
        ph = np.zeros((M, K, 3))                                        # fase relativa por paso y qubit
        if m in 'AC':
            ph += rng.uniform(-3, 3, (M, 1, 3)) * DT
        if m in 'BC':
            ph += np.pi * (rng.random((M, K, 3)) < 0.02)
        if m == 'D':
            s = np.where(rng.random((M, 3)) < 0.5, 1.0, -1.0)
            for k in range(K):
                ph[:, k, :] += 2 * s * DT
                s = np.where(rng.random((M, 3)) < 0.05, -s, s)
        if m == 'E':
            ph += np.where(rng.random((M, K, 3)) < 0.5, 3.0, -3.0) * DT
        out[m] = (abs(np.exp(1j * ph.sum((1, 2))).mean()), abs(np.exp(1j * (ph * sgn).sum((1, 2))).mean()))
    for m in 'FG':                                                     # sistema ⊗ entorno, sin QASM
        out[m] = tuple(env_model(m, eco) for eco in (0, 1))
    return out


def env_model(m, eco):
    """GHZ ⊗ |+++>, cu1(π/8) por paso con el entorno, eco con X; r = |<000|ρ|111>|·2"""
    plus = np.array([1, 1]) / np.sqrt(2)
    S = np.diag([1, np.exp(1j * np.pi / 8)])                           # fase del entorno si el qubit está en 1
    H = np.array([[1, 1], [1, -1]]) / np.sqrt(2)
    env = {0: plus.astype(complex), 1: plus.astype(complex)}           # entorno de un qubit, rama q=0 y q=1
    flipped = False
    for k in range(K):
        for b in (0, 1):
            if (b == 1) != flipped:                                    # el qubit vale 1 en esta rama
                env[b] = S @ env[b]
        if k == K // 2 - 1:
            if eco:
                flipped = not flipped
            if m == 'G':
                env = {b: H @ v for b, v in env.items()}
    if eco:
        flipped = not flipped
    return abs(np.vdot(env[0], env[1])) ** 3                           # tres qubits independientes


def main():
    tool = sys.argv[1]
    cf = closed_forms()
    report(all(abs(cf[m][i] - PREREG[m][i]) < 6e-5 for m in MODELS for i in (0, 1)),
           'formas cerradas = tabla del prerregistro (4 decimales)')

    raw = {}
    for line in subprocess.run([tool, 'raw'], check=True, capture_output=True, text=True).stdout.splitlines():
        m, rl, re_ = line.split()
        raw[m] = (float(rl), float(re_))
    same, st = same_circuits(tool)
    worst = max(abs(same[m][i] - raw[m][i]) for m in MODELS for i in (0, 1))
    report(worst < 1e-9, f'numpy corre los {4 * 1000 * 5 + 8} QASM exportados y da el mismo r (máx |Δ| = {worst:.1e})')
    ok, n = check_stats(tool, st)
    report(ok, f'después del prerregistro: r ± σ y p de Rayleigh = numpy ({n} modelos, libre y eco)')

    mc = own_monte_carlo()
    det = max(abs(mc[m][i] - cf[m][i]) for m in 'FG' for i in (0, 1))
    report(det < 1e-12, f'F y G con el entorno explícito = forma cerrada (|Δ| = {det:.1e})')
    worst = max(abs(mc[m][i] - cf[m][i]) for m in 'ABCDE' for i in (0, 1))
    report(worst < 0.03, f'Monte Carlo propia (20000, otro generador) = forma cerrada (máx |Δ| = {worst:.3f})')
    sys.exit(fail)


if __name__ == '__main__':
    main()

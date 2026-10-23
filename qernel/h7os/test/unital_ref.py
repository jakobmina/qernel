#!/usr/bin/env python3
"""
unital_ref.py — verificación independiente de la prueba de unitalidad.

  python3 test/unital_ref.py build/unital_exp

Tres caminos, ninguno con el código C del simulador:
  1. formas cerradas: recalcula las predicciones del prerregistro;
  2. los mismos circuitos: corre en numpy (qasm_ref.Sim) todos los QASM que
     exporta unital_exp y reconstruye t, w, z0, z1 (±1e-9 contra `unital_exp raw`);
  3. otra implementación: los seis canales con matrices de densidad, sin QASM.
     El intercambio sale de exponenciar XX+YY, el entorno caliente es I/2 de
     verdad (no 256 configuraciones), el demonio es su unitario conjunto con el
     medidor, y A y B usan una Monte Carlo propia con el generador de numpy.
Sale con 0 si todo coincide.
"""
import os
import re
import subprocess
import sys
from functools import reduce

import numpy as np

sys.path.insert(0, os.path.dirname(__file__))
from qasm_ref import Sim  # noqa: E402

K, TH = 8, np.pi / 8
MODELS = 'ABCDEF'
fail = 0

I2 = np.eye(2, dtype=complex)
X = np.array([[0, 1], [1, 0]], dtype=complex)
Y = np.array([[0, -1j], [1j, 0]])
Z = np.diag([1.0, -1.0]).astype(complex)


def report(ok, text):
    global fail
    print(('ok    ' if ok else 'FALLO ') + text)
    fail |= not ok


# --- 1. formas cerradas ------------------------------------------------------

def closed_forms():
    c16 = np.cos(TH) ** 16
    s16 = (2 * np.sin(0.5)) ** 16
    return {'A': (c16, -c16, 0.0), 'B': (s16, -s16, 0.0), 'C': (1.0, -1.0, 0.0), 'D': (c16, -c16, 0.0),
            'E': (1.0, 1 - 2 * c16, 1 - c16), 'F': (1.0, 1 - 2 * c16, 1 - c16)}


PREREG = {'A': (0.2817, -0.2817, 0), 'B': (0.5105, -0.5105, 0), 'C': (1, -1, 0), 'D': (0.2817, -0.2817, 0),
          'E': (1, 0.4365, 0.7183), 'F': (1, 0.4365, 0.7183)}


# --- 2. los mismos circuitos en numpy -------------------------------------

def zz(sim, regs):
    odd = np.zeros(len(sim.idx), dtype=int)
    for r in regs:
        base, size = sim.q[r]
        for k in range(base, base + size):
            odd ^= (sim.idx >> k) & 1
    return float(np.sum(np.abs(sim.psi) ** 2 * (1 - 2 * odd)))


def same_circuits(tool):
    out = {}
    for m in MODELS:
        per = []
        for b in 'xyz':
            text = subprocess.run([tool, 'dump', m, b], check=True, capture_output=True, text=True).stdout
            circuits = [c for c in re.split(r'(?=^OPENQASM)', text, flags=re.M) if c.strip()]
            vals = []
            for c in circuits:
                sim = Sim(c, stop_at_measure=True).run()
                vals.append((zz(sim, ['q']), zz(sim, ['q', 'r'])))
            per.append(np.array(vals))
        t = np.array([per[b][:, 0].mean() for b in range(3)])
        w = per[2][:, 1].mean()
        tmax = np.max(np.sqrt(sum(per[b][:, 0] ** 2 for b in range(3))))
        out[m] = dict(t=t, w=w, z0=t[2] + w, z1=t[2] - w, tn=np.linalg.norm(t), tmax=tmax, n=3 * len(per[0]))
    return out


# --- 3. canales con matrices de densidad, sin QASM ---------------------------

def bloch(rho):
    return np.real([np.trace(rho @ P) for P in (X, Y, Z)])


def ptrace_env(rho4):                       # sistema ⊗ entorno -> sistema
    return np.einsum('aibi->ab', rho4.reshape(2, 2, 2, 2))


def exchange():
    w, v = np.linalg.eigh(np.kron(X, X) + np.kron(Y, Y))
    return v @ np.diag(np.exp(-1j * TH * w / 2)) @ v.conj().T


def demon():                                # cry(2θ) sistema→medidor, luego cx medidor→sistema
    c, s = np.cos(TH), np.sin(TH)
    ry = np.array([[c, -s], [s, c]], dtype=complex)
    P0, P1 = np.diag([1, 0]).astype(complex), np.diag([0, 1]).astype(complex)
    cry = np.kron(P0, I2) + np.kron(P1, ry)                  # orden: sistema ⊗ medidor
    cx_ms = np.kron(I2, P0) + np.kron(X, P1)
    return cx_ms @ cry


def collide(rho, U, env):
    for _ in range(K):
        rho = ptrace_env(U @ np.kron(rho, env) @ U.conj().T)
    return rho


def channel_outputs(m, rng, M=20000):
    """(z0, z1, t) con entradas |0>, |1> e I/2"""
    ins = [np.diag([1, 0]).astype(complex), np.diag([0, 1]).astype(complex), I2 / 2]
    if m in 'AB':
        p = np.sin(TH) ** 2 / 2
        acc = [np.zeros((2, 2), complex) for _ in ins]
        tmax = 0.0
        for _ in range(M):
            U = I2
            for _ in range(K):
                if m == 'A':
                    if rng.random() < p:
                        U = X @ U
                else:
                    a, b, g = rng.uniform(-0.5, 0.5, 3)
                    rx = np.cos(a / 2) * I2 - 1j * np.sin(a / 2) * X
                    ry = np.cos(b / 2) * I2 - 1j * np.sin(b / 2) * Y
                    rz = np.cos(g / 2) * I2 - 1j * np.sin(g / 2) * Z
                    U = rz @ ry @ rx @ U
            outs = [U @ r @ U.conj().T for r in ins]
            tmax = max(tmax, np.linalg.norm(bloch(outs[2])))
            for k in range(3):
                acc[k] += outs[k] / M
        outs = acc
    else:
        tmax = None
        if m == 'C':                          # cu1(π/8)^8 = CZ con el entorno en |+>
            plus = np.full((2, 2), 0.5, dtype=complex)
            U = np.diag([1, 1, 1, np.exp(1j * TH)]) ** K
            outs = [ptrace_env(U @ np.kron(r, plus) @ U.conj().T) for r in ins]
        elif m == 'D':
            outs = [collide(r, exchange(), I2 / 2) for r in ins]
        elif m == 'E':
            outs = [collide(r, exchange(), np.diag([1, 0]).astype(complex)) for r in ins]
        else:
            outs = [collide(r, demon(), np.diag([1, 0]).astype(complex)) for r in ins]
    b = [bloch(o) for o in outs]
    return b[0][2], b[1][2], b[2], tmax


def kraus_ad(rho):
    K0 = np.diag([1, np.cos(TH)]).astype(complex)
    K1 = np.array([[0, np.sin(TH)], [0, 0]], dtype=complex)
    for _ in range(K):
        rho = K0 @ rho @ K0.conj().T + K1 @ rho @ K1.conj().T
    return rho


def main():
    tool = sys.argv[1]
    cf = closed_forms()
    report(all(abs(cf[m][i] - PREREG[m][i]) < 6e-5 for m in MODELS for i in range(3)),
           'formas cerradas = tabla del prerregistro (4 decimales)')

    raw = {}
    for line in subprocess.run([tool, 'raw'], check=True, capture_output=True, text=True).stdout.splitlines():
        f = line.split()
        v = list(map(float, f[1:]))
        raw[f[0]] = dict(t=np.array(v[0:3]), w=v[3], z0=v[4], z1=v[5], tn=v[6], tmax=v[7])
    same = same_circuits(tool)
    worst = max(max(np.max(np.abs(same[m]['t'] - raw[m]['t'])),
                    *(abs(same[m][k] - raw[m][k]) for k in ('w', 'z0', 'z1', 'tn', 'tmax'))) for m in MODELS)
    n = sum(same[m]['n'] for m in MODELS)
    report(worst < 1e-9, f'numpy corre los {n} QASM exportados y da los mismos t, w, z0, z1 (máx |Δ| = {worst:.1e})')
    report(all(same[m]['tmax'] < 1e-12 for m in 'ABC'),
           'numpy: |t| = 0 en cada realización de A, B y C, no solo en promedio')

    rng = np.random.default_rng(2026)
    worst_exact = worst_mc = 0.0
    tmax_ab = 0.0
    for m in MODELS:
        z0, z1, t, tmax = channel_outputs(m, rng)
        d = max(abs(z0 - cf[m][0]), abs(z1 - cf[m][1]), abs(np.linalg.norm(t) - cf[m][2]), abs(t[0]), abs(t[1]))
        if m in 'AB':
            worst_mc = max(worst_mc, d)
            tmax_ab = max(tmax_ab, tmax)
        else:
            worst_exact = max(worst_exact, d)
    report(worst_exact < 1e-12, f'C–F con matrices de densidad (entorno caliente = I/2 de verdad) '
                                f'= forma cerrada (|Δ| = {worst_exact:.1e})')
    report(worst_mc < 0.03 and tmax_ab < 1e-12, f'A y B: Monte Carlo propia (20000, otro generador) = forma '
                                                f'cerrada (máx |Δ| = {worst_mc:.3f}), |t| = 0 en cada una')
    rhos = [np.diag([1, 0]).astype(complex), np.diag([0, 1]).astype(complex), np.array([[0.5, 0.5], [0.5, 0.5]])]
    dk = max(np.max(np.abs(kraus_ad(r) - collide(r, demon(), np.diag([1, 0]).astype(complex)))) for r in rhos)
    de = max(np.max(np.abs(kraus_ad(r) - collide(r, exchange(), np.diag([1, 0]).astype(complex)))) for r in rhos)
    report(dk < 1e-12 and de < 1e-12, f'demonio = entorno frío = amortiguamiento de amplitud, también en la '
                                      f'coherencia de |+> (|Δ| = {max(dk, de):.1e})')
    sys.exit(fail)


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""
qasm_fuzz.py — circuitos OpenQASM al azar (todas las puertas del vector de
estado, ángulos como expresiones) corridos por el kernel (build/qasm_sv) y por
numpy (test/qasm_ref.py); las amplitudes deben coincidir.

  python3 test/qasm_fuzz.py build/qasm_sv [circuitos]
"""
import os
import random
import subprocess
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(__file__))
from qasm_ref import Sim  # noqa: E402

G1 = ['h', 's', 'sdg', 't', 'tdg', 'x', 'y', 'z', 'id']
GA = ['rz', 'rx', 'ry', 'u1', 'p']
G2 = ['cx', 'cz']
G2A = ['cu1', 'cp']


def angle_expr(rng):
    forms = [
        lambda: f'{rng.uniform(-7, 7):.6f}',
        lambda: f'pi/{rng.randint(1, 16)}',
        lambda: f'-{rng.randint(1, 5)}*pi/{rng.randint(1, 9)}',
        lambda: f'{rng.randint(-99999, 99999)}/{2 ** rng.randint(0, 27)}',
        lambda: f'(pi+{rng.randint(1, 9)})/{rng.randint(2, 7)}-0.{rng.randint(0, 999):03d}',
        lambda: f'{rng.uniform(-1, 1):.3e}',
        lambda: f'{rng.randint(1, 200)}*pi',
    ]
    return rng.choice(forms)()


def circuit(rng):
    regs = [('q', rng.randint(1, 4)), ('r', rng.randint(1, 4))]
    lines = ['OPENQASM 2.0;', 'include "qelib1.inc";']
    lines += [f'qreg {n}[{k}];' for n, k in regs]
    lines.append('creg c[1];')
    qs = [f'{n}[{i}]' for n, k in regs for i in range(k)]
    for _ in range(rng.randint(5, 40)):
        kind = rng.random()
        if kind < 0.35:
            lines.append(f'{rng.choice(G1)} {rng.choice(qs + [regs[0][0]])};')
        elif kind < 0.65:
            lines.append(f'{rng.choice(GA)}({angle_expr(rng)}) {rng.choice(qs)};')
        elif len(qs) > 1:
            a, b = rng.sample(qs, 2)
            if kind < 0.85:
                lines.append(f'{rng.choice(G2)} {a},{b};')
            else:
                lines.append(f'{rng.choice(G2A)}({angle_expr(rng)}) {a},{b};')
    lines.append('measure q[0] -> c[0];')
    return '\n'.join(lines) + '\n'


def main():
    tool, n = sys.argv[1], int(sys.argv[2]) if len(sys.argv) > 2 else 300
    rng = random.Random(7)
    worst = 0.0
    for k in range(n):
        text = circuit(rng)
        out = subprocess.run([tool], input=text, capture_output=True, text=True, check=True).stdout
        c = np.array([complex(float(a), float(b)) for a, b in (l.split() for l in out.splitlines())])
        ref = Sim(text, stop_at_measure=True).run().psi
        err = float(np.max(np.abs(c - ref)))
        worst = max(worst, err)
        if err > 1e-12:
            print(f'circuito {k}: diferencia {err:.2e}\n{text}')
            sys.exit(1)
    print(f'{n} circuitos al azar: amplitudes kernel = numpy (máx |Δ| = {worst:.1e})')


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""
qasm_ref.py — referencia independiente: corre OpenQASM 2.0 con un simulador de
vector de estado en numpy (2^n amplitudes complejas), no con un tablero.

  python3 test/qasm_ref.py circuito.qasm      síndromes y lectura de un circuito
  python3 test/qasm_ref.py --sweep GEN        el mismo barrido que `q sweep`,
                                              pidiendo cada circuito a GEN (steane_gen)
  python3 test/qasm_ref.py --amps < c.qasm    amplitudes justo antes de la primera medición

Puertas: h s sdg t tdg x y z id, rz rx ry u1 p (con ángulo), cx cz cu1 cp,
measure, reset, if. Los ángulos se evalúan con aritmética de Python
(números, pi, + - * / y paréntesis).

Comprueba además que cada medición de síndrome sea determinista (probabilidad
0 o 1): si no lo fuera, el código no estaría midiendo un estabilizador.
"""
import math
import re
import subprocess
import sys

import numpy as np

S2 = 1 / np.sqrt(2)
GATES = {
    'h':   np.array([[S2, S2], [S2, -S2]], dtype=complex),
    's':   np.array([[1, 0], [0, 1j]], dtype=complex),
    'sdg': np.array([[1, 0], [0, -1j]], dtype=complex),
    'x':   np.array([[0, 1], [1, 0]], dtype=complex),
    'y':   np.array([[0, -1j], [1j, 0]], dtype=complex),
    'z':   np.array([[1, 0], [0, -1]], dtype=complex),
    'id':  np.eye(2, dtype=complex),
    't':   np.diag([1, np.exp(1j * np.pi / 4)]),
    'tdg': np.diag([1, np.exp(-1j * np.pi / 4)]),
}


def rot(g, a):
    """puertas de un qubit con ángulo, como en qelib1.inc"""
    c, s = np.cos(a / 2), np.sin(a / 2)
    if g == 'rz':
        return np.diag([np.exp(-1j * a / 2), np.exp(1j * a / 2)])
    if g in ('u1', 'p'):
        return np.diag([1, np.exp(1j * a)])
    if g == 'rx':
        return np.array([[c, -1j * s], [-1j * s, c]])
    if g == 'ry':
        return np.array([[c, -s], [s, c]], dtype=complex)
    raise KeyError(g)


def angle(text):
    if not re.fullmatch(r'[0-9.eE+\-*/() pi]+', text):
        raise ValueError(f'ángulo raro: {text}')
    return float(eval(text, {'__builtins__': {}}, {'pi': math.pi}))


class Sim:
    def __init__(self, text, seed=1, stop_at_measure=False):
        self.stop_at_measure = stop_at_measure
        self.stopped = False
        text = re.sub(r'//[^\n]*', '', text)
        self.stmts = [s.strip() for s in text.split(';') if s.strip()]
        self.q, self.c = {}, {}
        nq = nc = 0
        for s in self.stmts:
            m = re.fullmatch(r'(qreg|creg)\s+(\w+)\s*\[(\d+)\]', s)
            if m:
                size = int(m[3])
                if m[1] == 'qreg':
                    self.q[m[2]] = (nq, size); nq += size
                else:
                    self.c[m[2]] = (nc, size); nc += size
        self.n = nq
        self.psi = np.zeros(2 ** nq, dtype=complex)
        self.psi[0] = 1
        self.bits = [0] * nc
        self.rng = np.random.default_rng(seed)
        self.idx = np.arange(2 ** nq)
        self.probs = []                      # probabilidad de 1 en cada medición, en orden

    def qubits(self, op):
        m = re.fullmatch(r'(\w+)\s*(?:\[(\d+)\])?', op.strip())
        base, size = self.q[m[1]]
        return [base + int(m[2])] if m[2] is not None else list(range(base, base + size))

    def cbits(self, op):
        m = re.fullmatch(r'(\w+)\s*(?:\[(\d+)\])?', op.strip())
        base, size = self.c[m[1]]
        return [base + int(m[2])] if m[2] is not None else list(range(base, base + size))

    def creg(self, name):
        base, size = self.c[name]
        return sum(self.bits[base + i] << i for i in range(size))

    def gate1(self, u, k):
        psi = self.psi.reshape(2 ** (self.n - k - 1), 2, 2 ** k)
        self.psi = np.einsum('ij,ajb->aib', u, psi).reshape(-1)

    def cx(self, c, t):
        sel = self.idx[(((self.idx >> c) & 1) == 1) & (((self.idx >> t) & 1) == 0)]
        other = sel | (1 << t)
        self.psi[sel], self.psi[other] = self.psi[other].copy(), self.psi[sel].copy()

    def measure(self, k):
        one = ((self.idx >> k) & 1) == 1
        p1 = float(np.sum(np.abs(self.psi[one]) ** 2))
        self.probs.append(p1)
        out = int(self.rng.random() < p1)
        keep = one if out else ~one
        self.psi[~keep] = 0
        self.psi /= np.linalg.norm(self.psi)
        return out

    def execute(self, s):
        m = re.fullmatch(r'if\s*\(\s*(\w+)\s*==\s*(\d+)\s*\)\s*(.*)', s, re.S)
        if m:
            if self.creg(m[1]) == int(m[2]):
                self.execute(m[3].strip())
            return
        m = re.fullmatch(r'measure\s+(.+?)\s*->\s*(.+)', s, re.S)
        if m:
            if self.stop_at_measure:
                self.stopped = True
                return
            for q, c in zip(self.qubits(m[1]), self.cbits(m[2])):
                self.bits[c] = self.measure(q)
            return
        m = re.fullmatch(r'(\w+)\s*(?:\((.*)\))?\s+([^()]+)', s, re.S)
        g, args = m[1], [a.strip() for a in m[3].split(',')]
        ang = [angle(a) for a in m[2].split(',')] if m[2] is not None else []
        if g in ('cu1', 'cp'):
            a, b = self.qubits(args[0])[0], self.qubits(args[1])[0]
            both = (((self.idx >> a) & 1) == 1) & (((self.idx >> b) & 1) == 1)
            self.psi[both] *= np.exp(1j * ang[0])
        elif g in ('cx', 'cz'):
            a, b = self.qubits(args[0])[0], self.qubits(args[1])[0]
            if g == 'cz':
                self.gate1(GATES['h'], b)
            self.cx(a, b)
            if g == 'cz':
                self.gate1(GATES['h'], b)
        elif g == 'reset':
            for q in self.qubits(args[0]):
                if self.measure(q):
                    self.gate1(GATES['x'], q)
        else:
            u = rot(g, ang[0]) if ang else GATES[g]
            for q in self.qubits(args[0]):
                self.gate1(u, q)

    def run(self):
        for s in self.stmts:
            if re.match(r'(OPENQASM|include|qreg|creg|barrier)\b', s):
                continue
            self.execute(s)
            if self.stopped:
                break
        return self

    def parity(self, reg):
        """<Z...Z> sobre todos los qubits del registro, con el estado actual"""
        base, size = self.q[reg]
        odd = np.zeros(len(self.idx), dtype=int)
        for k in range(base, base + size):
            odd ^= (self.idx >> k) & 1
        return float(np.sum(np.abs(self.psi) ** 2 * (1 - 2 * odd)))


def steane(text, seed=1):
    sim = Sim(text, seed).run()
    sz, sx, m = sim.creg('sz'), sim.creg('sx'), sim.creg('m')
    msyn, par = 0, 0
    for k in range(7):
        if (m >> k) & 1:
            msyn ^= k + 1
            par ^= 1
    det = all(p < 1e-9 or p > 1 - 1e-9 for p in sim.probs[:6])    # las 6 de síndrome
    return sz, sx, m, msyn, par, det


def gen(tool, plus, errs):
    args = [tool] + (['+'] if plus else []) + [t for p, j in errs for t in (p, str(j))]
    return subprocess.run(args, check=True, capture_output=True, text=True).stdout


def sweep(tool):
    P = 'xyz'
    base_ok, ok1, alldet = True, [0, 0], True
    for plus in (0, 1):
        sz, sx, m, msyn, par, det = steane(gen(tool, plus, []))
        base_ok &= (sz, sx, par, msyn) == (0, 0, 0, 0)
        alldet &= det
        for p in P:
            for j in range(1, 8):
                sz, sx, m, msyn, par, det = steane(gen(tool, plus, [(p, j)]))
                want = (j if p != 'z' else 0, j if p != 'x' else 0)
                ok1[plus] += (sz, sx) == want and par == 0 and msyn == 0
                alldet &= det
    ok2 = tot2 = 0
    for i in range(1, 8):
        for j in range(i + 1, 8):
            for p in P:
                for q in P:
                    a = steane(gen(tool, 0, [(p, i), (q, j)]))
                    b = steane(gen(tool, 1, [(p, i), (q, j)]))
                    ok2 += a[4] == 0 and b[4] == 0
                    alldet &= a[5] and b[5]
                    tot2 += 1
    print(f"sin error: síndromes 000/000 y lógico 0 en ambas bases: {'sí' if base_ok else 'NO'}")
    print(f"1 error:   {ok1[0]}/21 corregidos en |0_L> · {ok1[1]}/21 en |+_L>")
    print(f"2 errores: {ok2}/{tot2} sin error lógico · {tot2 - ok2}/{tot2} con error lógico (d = 3)")
    print(f"síndromes deterministas en los {2 + 42 + 2 * tot2} circuitos: {'sí' if alldet else 'NO'}",
          file=sys.stderr)


if __name__ == '__main__':
    if len(sys.argv) == 2 and sys.argv[1] == '--amps':
        sim = Sim(sys.stdin.read(), stop_at_measure=True).run()
        for a in sim.psi:
            print(f'{a.real:.17g} {a.imag:.17g}')
    elif len(sys.argv) == 3 and sys.argv[1] == '--sweep':
        sweep(sys.argv[2])
    elif len(sys.argv) == 2:
        sz, sx, m, msyn, par, det = steane(open(sys.argv[1]).read())
        print(f"sz {sz:03b} · sx {sx:03b} · síndrome clásico {msyn:03b} · paridad {par} · "
              f"síndromes deterministas: {'sí' if det else 'NO'}")
    else:
        print(__doc__)
        sys.exit(2)

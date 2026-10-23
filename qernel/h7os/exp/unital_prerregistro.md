# Prueba de unitalidad — prerregistro

Escrito el 2026-10-07, antes de que exista el código que lo corre. La huella
SHA-256 se guarda en `exp/unital_prerregistro.sha256`; este archivo no se edita
después. Las desviaciones se anotan en `exp/unital_resultados.md`.

## Pregunta

El experimento del eco mostró que, con desfase puro, un entorno cuántico y un
campo clásico dan los mismos números. ¿Lo separa la **unitalidad**?

Un canal es unital si deja fijo el estado maximalmente mezclado: Λ(I/2) = I/2.

## Lo que ya es teorema (no se pone a prueba)

- Un campo clásico es una mezcla de unitarios que no dependen del estado del
  sistema: Λ(ρ) = Σ p_k U_k ρ U_k†.
  - Cada U_k deja fijo I/2.
  - Por lo tanto el vector t (el vector de Bloch de Λ(I/2)) vale 0 en **cada
    realización**, no solo en promedio.
- En un qubit vale también al revés: todo canal unital es una mezcla de
  unitarios.
  - Con rotaciones, el mapa de Bloch se lleva a una forma diagonal.
  - La positividad completa la confina al tetraedro de los canales de Pauli.
  - En dimensión ≥ 3 esto falla (canal de Landau–Streater).
- Entonces, para un qubit: **|t| > 0 ⟺ ningún campo clásico explica el canal.**
- Partiendo de I/2 (1 bit de entropía), la entropía de salida es
  S = h((1 + |t|)/2).
  - Un canal unital nunca la baja de 1 bit.
  - Uno no unital sí: el entorno le saca entropía al sistema.

## Sistema y lectura

- Un qubit de sistema `q[0]` y una referencia `r[0]` en un par de Bell:
  `h q[0]; cx q[0],r[0];`.
  - El estado reducido de `q[0]` es exactamente I/2.
  - El ruido nunca toca `r[0]`.
- K = 8 pasos.
- Lectura de `q[0]`:
  - X: `h q[0];`
  - Y: `sdg q[0]; h q[0];`
  - Z: sin rotación.
  - Después `measure q[0] -> c[0]; measure r[0] -> c[1];`.

  Se calculan valores esperados exactos justo antes de medir.
- Estadísticos, promediados sobre las M realizaciones:
  - t = (⟨X_q⟩, ⟨Y_q⟩, ⟨Z_q⟩);
  - w = ⟨Z_q Z_r⟩.
- Como la referencia recuerda la entrada (estado de Choi):
  - z0 = t_z + w es ⟨Z⟩ de salida si la entrada fue |0⟩;
  - z1 = t_z − w es ⟨Z⟩ de salida si la entrada fue |1⟩.
- |t| mide la no unitalidad.
- Cada realización tiene su semilla, que depende solo de (modelo, i); las tres
  lecturas usan las mismas realizaciones.

## Modelos

| | modelo | tipo | en cada paso k | M |
|---|---|---|---|---|
| A | patadas X | campo clásico | `x q[0]` con probabilidad p = sin²(π/8)/2 ≈ 0.0732 | 1000 |
| B | campo 3D al azar | campo clásico | `rx(α) ry(β) rz(γ) q[0]`, ángulos uniformes en [−1/2, 1/2), nuevos en cada paso | 1000 |
| C | desfase cuántico | entorno cuántico | `e[0]` en \|+⟩; `cu1(pi/8) q[0],e[0]` (el modelo F del eco, con un solo qubit) | 1 |
| D | entorno caliente | entorno cuántico | `e[k]` en \|0⟩ o \|1⟩; intercambio parcial θ = π/8 entre `q[0]` y `e[k]`, un qubit nuevo por paso; se promedian exactamente las 256 configuraciones | 256 |
| E | entorno frío | entorno cuántico | igual que D, con todos los `e[k]` en \|0⟩ | 1 |
| F | demonio | registro clásico | medidor `m[k]` en \|0⟩: medición débil `cry(π/4) q[0],m[k]` y corrección `cx m[k],q[0]` | 1 |

**Intercambio parcial**, exp(−iθ(XX+YY)/2) con θ = π/8: es la interacción de
intercambio de energía. Se escribe así (identidad comprobada con matrices antes
de este registro):

```
cx q[0],e[k]; h q[0]; rz(pi/8) q[0]; cx e[k],q[0]; rz(-pi/8) q[0]; cx e[k],q[0]; h q[0]; cx q[0],e[k];
```

**Medición débil del demonio**, `cry(π/4)`:

```
ry(pi/8) m[k]; cx q[0],m[k]; ry(-pi/8) m[k]; cx q[0],m[k];
```

- Después viene `cx m[k],q[0]`. Por el principio de medición diferida, eso
  equivale a medir `m[k]` y aplicar `x q[0]` si salió 1.
- `m[k]` no se vuelve a tocar, así que su contenido es un bit clásico: el
  registro del demonio.

## Predicciones (formas cerradas)

| | z0 | z1 | \|t\| | clase | de dónde sale |
|---|---|---|---|---|---|
| A | 0.2817 | −0.2817 | 0 | unital | (1 − 2p)^8 = cos^16(π/8) |
| B | 0.5105 | −0.5105 | 0 | unital | E[R] por paso = s²·I, con s = 2 sin(1/2); total s^16 |
| C | 1 | −1 | 0 | unital | el desfase no toca poblaciones |
| D | 0.2817 | −0.2817 | 0 | unital | amortiguamiento generalizado con N = 1/2 por paso: z → cos²(π/8)·z |
| E | 1 | 0.4365 | 0.7183 | no unital | amortiguamiento de amplitud, γ = sin²(π/8) por paso: z1 = 1 − 2cos^16, \|t\| = 1 − cos^16 |
| F | 1 | 0.4365 | 0.7183 | no unital | Kraus del demonio: K0 = diag(1, cos θ), K1 = sin θ \|0⟩⟨1\|; es el mismo canal que E |

Valores exactos: cos^16(π/8) = 0.28173806968950743 y s^16 = 0.5105268265463945.

Tolerancias:
- |t|: ±1e-9 en todos los modelos.
- z0 y z1:
  - ±1e-9 en C–F;
  - ±0.15 en A y B, que son muestreados (σ ≈ 0.03).

Predicciones exactas adicionales:
- t_x = t_y = 0 (±1e-9) en todos.
- E y F coinciden en z0, z1 y |t| (±1e-9), aunque sean circuitos distintos.
- En A, B y C, |t| es 0 en cada realización. En D solo lo es el promedio de
  las 256 configuraciones.

Regla de clasificación: **no unital** si |t| > 1e-9; **unital** en otro caso.
La clasificación no tiene azar: en A y B, t = 0 en cada realización.

## Hipótesis

- **H-ingenua-2**: la no unitalidad separa entorno cuántico (C, D, E) de clásico
  (A, B, F).
  - Criterio de refutación, fijado ahora: queda refutada si
    - D, que es cuántico, sale unital, y
    - F, que es un registro clásico, sale no unital con |t_F − t_E| ≤ 1e-9.
- **H-sumidero** (la predicha): |t| > 0 exactamente cuando el entorno funciona
  como sumidero de entropía. Para eso necesita dos cosas:
  1. empezar con baja entropía (puro, o memoria en blanco);
  2. un acoplamiento que mueva las poblaciones del sistema según su estado.

  Cómo cae cada modelo:
  - C tiene lo primero sin lo segundo;
  - D tiene lo segundo sin lo primero;
  - E y F tienen las dos;
  - A y B son campos sin memoria.

  Predicción: E y F no unitales; A, B, C y D unitales.

**Por qué se espera que H-ingenua-2 falle:**
- Todo canal se puede realizar con un entorno que solo guarda un registro
  clásico. Cada descomposición de Kraus es una medición con resultado clásico
  seguida de una corrección.
- Ninguna prueba de un solo tiempo hecha sobre el sistema certifica que el
  entorno sea cuántico.
- Certificar memoria cuántica pide varias intervenciones y un testigo sobre el
  tensor de proceso (Giarmatzi y Costa, Quantum 5, 440, 2021). Queda fuera de
  este experimento.

## Qué contaría como error de implementación

- |t| ≠ 0 (±1e-9) en alguna realización de A, B o C.
- t_x o t_y ≠ 0.
- E ≠ F.
- z0 de E ≠ 1.
- El kernel y la herramienta de Linux dan números distintos.
- numpy, corriendo los mismos QASM, da otros valores (±1e-9).
- numpy, implementando los canales con matrices de densidad y operadores de
  Kraus sin QASM, se aleja de las formas cerradas.

## Verificación

1. `q unital` en el kernel y `tools/unital_exp` en Linux (mismo código C)
   imprimen tablas idénticas byte a byte.
2. `test/unital_ref.py`:
   - corre en numpy todos los QASM exportados;
   - implementa los seis canales con matrices de densidad, con su propia
     Monte Carlo para A y B;
   - evalúa las formas cerradas.
3. Los resultados se comparan con esta tabla en `exp/unital_resultados.md`.

# Prueba de unitalidad — resultados

**Prerregistro:**
- archivo `exp/unital_prerregistro.md`;
- escrito el 2026-10-07 a las 20:47 UTC, antes del código;
- SHA-256 `1d1799539bc375b65e4faab82ffecfea59cdf7affda62e17b75bf28b27044a7e`;
- `make test` comprueba que no cambió.

Se corrió con `q unital` dentro de H7-OS en QEMU y con `tools/unital_exp` en
Linux. Las dos salidas son idénticas byte a byte.

## Predicho y medido

| | modelo | z0: predicho → medido | z1: predicho → medido | \|t\|: predicho → medido | clase |
|---|---|---|---|---|---|
| A | patadas X (clásico) | 0.2817 → 0.2960 | −0.2817 → −0.2960 | 0 → 0 | unital, como se predijo |
| B | campo 3D al azar (clásico) | 0.5105 → 0.4808 | −0.5105 → −0.4808 | 0 → 8e-19 | unital, como se predijo |
| C | desfase cuántico | 1 → 1.0000 | −1 → −1.0000 | 0 → 0 | unital, como se predijo |
| D | entorno caliente (cuántico) | 0.2817 → 0.2817 | −0.2817 → −0.2817 | 0 → 5e-18 | unital, como se predijo |
| E | entorno frío (cuántico) | 1 → 1.0000 | 0.4365 → 0.4365 | 0.7183 → 0.7183 | no unital, como se predijo |
| F | demonio (registro clásico) | 1 → 1.0000 | 0.4365 → 0.4365 | 0.7183 → 0.7183 | no unital, como se predijo |

- **Clases como se predijo: 6/6.** Números en tolerancia: 6/6.
- **Comprobaciones exactas:** todas pasan.
  - t_x = t_y = 0.
  - |t| = 0 en *cada* realización de A, B y C, no solo en promedio.
  - E = F en los tres números, con |Δ| ≈ 2e-15.
- **H-ingenua-2 queda refutada por el criterio fijado de antemano.**
  - D, que es cuántico, sale unital.
  - F, que es un registro clásico, da lo mismo que E.
- **Lo muestreado está en el ruido esperado.**
  - En A y B, z0 se aleja 0.014 y 0.030 de la forma cerrada (σ ≈ 0.03).
  - La Monte Carlo propia de numpy, con 20 000 realizaciones, cae a 0.004.
- **En D, el cero sale de un promedio.** Cada configuración del entorno caliente
  sí empuja al sistema: la configuración con todos los qubits en |0⟩ es
  exactamente E, y la de todos en |1⟩ empuja igual hacia |1⟩. Solo el promedio
  de las 256 se cancela.

**Verificación independiente** (`test/unital_ref.py`):
- numpy corre los 6777 QASM exportados y da los mismos t, w, z0 y z1:
  |Δ| ≤ 3.8e-15.
- Una segunda implementación, con matrices de densidad y sin QASM, coincide con
  las formas cerradas a 4e-16:
  - el intercambio sale de exponenciar XX+YY;
  - el entorno caliente es I/2 de verdad, no 256 configuraciones;
  - el demonio es su unitario conjunto con el medidor.
- El demonio y el entorno frío dan el mismo canal (amortiguamiento de amplitud),
  incluso en la coherencia de |+⟩: |Δ| = 1.6e-15.

## Qué se aprendió

**La unitalidad separa sumideros de entropía, no cuántico de clásico.** El
diseño 2×2 lo muestra:

| | acoplamiento que no mueve poblaciones | acoplamiento que mueve poblaciones |
|---|---|---|
| **entorno con alta entropía** (o un campo sin memoria) | A, B (campos) | D (caliente): unital |
| **entorno con baja entropía** (puro o en blanco) | C (desfase): unital | **E (frío), F (demonio): no unital** |

Solo cuando están las dos cosas, el entorno le saca entropía al sistema:
- partiendo de I/2 (1 bit), E y F dejan S = h(0.859) = 0.587 bits;
- todos los demás dejan 1 bit.

**Por qué no puede separar cuántico de clásico:**
- Todo canal se puede realizar con un entorno que solo guarda un registro
  clásico: cada descomposición de Kraus es una medición con resultado clásico
  seguida de una corrección.
- F es literalmente esa construcción para el amortiguamiento de amplitud:
  - medición débil con K0 = diag(1, cos θ);
  - si el medidor marca 1, corrección X;
  - el medidor queda con un bit clásico.
- El entorno cuántico frío E da el mismo canal hasta la última cifra.

## Lo que dejan claro los dos experimentos juntos

| prueba | qué mide del entorno | separa cuántico de clásico |
|---|---|---|
| r con y sin eco | memoria: si la segunda mitad recuerda a la primera | no |
| unitalidad, \|t\| | si es sumidero de entropía (frío o memoria en blanco) | no |

Con medidas sobre el sistema en uno o dos tiempos, ninguna de las dos certifica
un entorno cuántico, y no por falta de precisión: siempre existe una versión con
registros clásicos.

**Siguiente paso, si se quiere esa frontera:** certificar memoria cuántica.
- Hacen falta varias intervenciones sobre el sistema y un testigo de
  entrelazamiento sobre el **tensor de proceso** (Giarmatzi y Costa, Quantum 5,
  440, 2021).
- El eco ya era un protocolo de dos tiempos. La versión fuerte reemplaza el `x`
  del medio por un conjunto completo de intervenciones y reconstruye el proceso.

## Desviaciones

- La probabilidad de patada X es un umbral entero sobre sorteos de 24 bits:
  1228483/2²⁴ = 0.07322329, contra la prerregistrada 0.07322330
  (diferencia de 1e-8).
- `echo.c` pasó sus utilidades de texto y azar a `src/qtext.c`, que ahora
  comparte con `unital.c`. Se comprobó que la salida del eco no cambió ni en un
  byte, tanto la tabla como los 1000 circuitos de E.
- Ninguna otra.

# Experimento del eco — resultados

**Prerregistro:**
- archivo `exp/eco_prerregistro.md`;
- escrito el 2026-10-07 a las 16:11 UTC, antes del código del experimento;
- SHA-256 `80a995f2267b6f6c47c29b0b46d9f6b23499462e97a5994bd82baac2e8b1838a`;
- `sha256sum -c exp/eco_prerregistro.sha256` → OK (lo comprueba `make test`).

Se corrió con `q echo` dentro de H7-OS en QEMU y con `tools/echo_exp` en Linux.
Las dos salidas son idénticas byte a byte.

## Predicho y medido

| | r libre: predicho → medido | r eco: predicho → medido | clase: predicha → medida |
|---|---|---|---|
| A campo estático | ≈ 0 (1.0e-4) → 0.0339 | 1 → 1.0000 | reversible → **reversible** |
| B patadas Z | 0.3754 → 0.3720 | 0.3754 → 0.3720 | irreversible → **irreversible** |
| C estático + patadas | ≈ 0 (3.9e-5) → 0.0152 | 0.3754 → 0.3540 | mixto → **mixto** |
| D telégrafo lento | ≈ 0 (0.0017) → 0.0067 | 0.5606 → 0.5599 | mixto → **mixto** |
| E telégrafo rápido | 0.1775 → 0.1785 | 0.1775 → 0.2018 | irreversible → **irreversible** |
| F entorno cuántico fijo | 0 → 0.0000 | 1 → 1.0000 | reversible → **reversible** |
| G entorno que cambia | 0.3536 → 0.3536 | 0.3536 → 0.3536 | irreversible → **irreversible** |

- **Clases como se predijo: 7/7.** Números dentro de la tolerancia: 7/7.
- **Comprobaciones exactas:** todas pasan.
  - A con eco da 1.
  - B libre = B eco.
  - F = (0, 1).
  - G = (1/(2√2), 1/(2√2)).
- **H-ingenua queda refutada por el criterio fijado de antemano.**
  - F cae dentro de la tolerancia de A en los dos números.
  - G cae en la misma clase que B y E.
- **Lo muestreado (A–E) es ruido de muestreo:**
  - Los valores «≈ 0» quedan en el piso esperable con M = 1000: para fases
    uniformes, |media| ≈ 0.028.
  - E con eco (0.2018) se aleja 0.024 de su forma cerrada. Con M = 1000, la
    desviación típica de cada r es ≈ 0.022.

**Verificación independiente** (`test/echo_ref.py`, sin el código C del simulador):
- numpy corre los 20008 circuitos QASM exportados y reconstruye los mismos r:
  máx |Δ| = 3.8e-15;
- implementa los siete modelos por su cuenta:
  - fases para los campos;
  - producto tensorial con el entorno para F y G;
- con su propio generador y 20000 realizaciones, cae a ≤ 0.010 de las formas
  cerradas.

Además, 300 circuitos al azar con todas las puertas dan las mismas amplitudes
en el vector de estado del kernel y en numpy (|Δ| ≤ 1.3e-15).

## Por qué falla H-ingenua (no es un accidente de estos números)

Con desfase puro, lo único que el sistema puede ver es, por cada qubit, el
solapamiento ⟨E₀|E₁⟩.
- E₀ y E₁ son los estados del entorno pegados a cada rama: la del qubit en 0
  y la del qubit en 1.
- Ese solapamiento es un número complejo de módulo ≤ 1.
- Promediar fases clásicas e^{iθ} produce el mismo tipo de número.

En estos modelos, la reproducción es exacta:

- **F es un campo clásico disfrazado.**
  - `cu1` es diagonal en la base Z del entorno, así que el valor Z de cada
    qubit del entorno nunca cambia: es un número, 0 o 1 al 50 %.
  - El qubit gana π/8 por paso solo si ese número vale 1.
  - Un campo estático clásico que vale 0 o π/8 por paso da:
    - libre: (1 + e^{iπ})/2 = 0;
    - con eco: 1.

    Es lo mismo que el entorno cuántico.
- **G es un campo que existe en la primera mitad y desaparece en la segunda.**
  - Fase θ₁ ∈ {0, π/2} al 50 %, y θ₂ = 0.
  - Por qubit da (1 + i)/2, de módulo 1/√2, y r = 1/(2√2), libre y con eco.
  - El entorno cuántico de G da exactamente (1 + i)/2 por qubit a mitad de
    camino, al final sin eco y al final con eco (comprobado en numpy).

Lo que el eco mide es **memoria**: si la fase de la segunda mitad está
correlacionada con la de la primera. La familia del telégrafo lo muestra con
un solo ruido clásico, cambiando solo su tiempo de correlación:
- sin cambios (A): reversible;
- cambio con probabilidad 0.05 por paso (D): mixto;
- cambio con probabilidad 0.5 (E): irreversible.

## Lo que sí era correcto de la intuición

1. **r mide entrelazamiento del sistema.**
   - El estado promediado tiene poblaciones solo en |000⟩ y |111⟩, y
     coherencia de módulo r/2.
   - Por el criterio de Gühne–Seevinck, r > 0 ⟺ entrelazamiento genuino de 3
     qubits, y la fidelidad con GHZ es (1 + r)/2.
   - r dice cuánto queda; no dice por qué se perdió.
2. **Kuramoto sí está en el experimento, a nivel 3, pero en el modelo A.**
   - Osciladores de Kuramoto sin acoplamiento (K = 0) con frecuencias
     repartidas son exactamente el campo estático: misma ecuación
     dθ/dt = ω_j, mismas unidades (rad/s) y misma interpretación (desintonía).
   - Su r decae por mezcla de fases y un eco lo recupera. Ott, Platig, Antonsen
     y Girvan describen ecos así en sistemas grandes de osciladores acoplados
     por debajo del acoplamiento crítico
     ([arXiv:0807.4499](https://arxiv.org/abs/0807.4499)).
   - Lo que Kuramoto no trae es la frontera cuántico/clásico: sus osciladores
     son clásicos y aun así reproducen F.

## Desviaciones y correcciones

- **Antes de correr el experimento** se encontró que `q_sincos` omitía el
  término r¹⁶ del coseno.
  - El error llegaba a 1e-15 (≈ 10 ulp).
  - Se detectó al comparar contra libm y se corrigió; ahora el error es
    ≤ 1.1e-16.
  - No afecta ninguna cifra publicada.
- **Probabilidades como umbrales enteros** sobre sorteos de 24 bits:
  - p_patada = 335544/2²⁴ = 0.01999998;
  - p_cambio = 838861/2²⁴ = 0.05000001.

  Difieren de las prerregistradas en menos de 2e-8.
- Ninguna otra: modelos, M, tolerancias y regla de clasificación son los del
  prerregistro.

## Qué deja claro el fallo

Con desfase puro, mirando solo el sistema, ningún número de este tipo separa un
entorno cuántico de un campo clásico. Para separarlos hace falta salir de
alguna de esas dos condiciones:

- **Salir del desfase puro.**
  - Un campo clásico actúa por unitarios al azar, y un promedio de unitarios
    deja fijo el estado maximalmente mezclado (canal unital).
  - Un entorno que intercambia energía con el sistema (relajación hacia |0⟩)
    no lo deja fijo.
  - Prueba concreta, en el mismo formato de prerregistro:
    1. preparar |0⟩ y |1⟩;
    2. dejarlos evolucionar;
    3. promediar las salidas.

    Si el resultado no es I/2, ningún campo clásico explica la dinámica.
- **Mirar también el entorno.**
  - En el simulador se puede medir directamente el entrelazamiento entre `q` y
    `e`: es la entropía del estado reducido.
  - En el laboratorio eso pide acceso al entorno, o protocolos con
    preparación del entorno.

## Análisis añadido en v0.7 (después del prerregistro)

No forma parte del prerregistro y no cambia nada de lo anterior: modelos, M,
tolerancias, regla de clasificación y criterio de refutación quedan igual.
`q echo` lo imprime después de las conclusiones prerregistradas.

**Pregunta:** ¿cada r muestreado es distinto de 0, o es solo el piso de
muestreo? Con M = 1000 fases al azar, |media| vale en promedio
≈ 0.886/√M ≈ 0.028, no 0.

**Método:**
- σ de r por el método delta: varianza de las c_i a lo largo de la media, entre M.
- p = exp(−M·r²): prueba de Rayleigh, la probabilidad de un r así de grande si
  el r verdadero fuera 0.
- Calibración, en una Monte Carlo aparte (4000 repeticiones de M = 1000): al
  nivel 0.05 dio 5.0 %, 5.0 % y 5.5 % de falsos positivos en A, C y D libres.
- `test/echo_ref.py` recalcula σ y p en numpy con los mismos QASM.

| | r libre ± σ | p | r eco ± σ | p |
|---|---|---|---|---|
| A | 0.0339 ± 0.0221 | 0.317 | 1.0000 ± 0.0000 | < 0.001 |
| B | 0.3720 ± 0.0294 | < 0.001 | 0.3720 ± 0.0294 | < 0.001 |
| C | 0.0152 ± 0.0223 | 0.793 | 0.3540 ± 0.0296 | < 0.001 |
| D | 0.0067 ± 0.0229 | 0.956 | 0.5599 ± 0.0179 | < 0.001 |
| E | 0.1785 ± 0.0220 | < 0.001 | 0.2018 ± 0.0211 | < 0.001 |
| F | 0 (exacto) | | 1 (exacto) | |
| G | 0.3536 (exacto) | | 0.3536 (exacto) | |

**Lectura:**
- A, C y D libres no se distinguen de 0, como predicen las formas cerradas
  (1.0e-4, 3.9e-5 y 0.0017). Su fidelidad GHZ (0.517, 0.508 y 0.503) queda por
  encima de 1/2 solo por el muestreo: no demuestra entrelazamiento genuino.
- E con eco se aleja 0.023 de E libre, ≈ 1σ. Con cambio de signo de
  probabilidad 0.5 por paso, el telégrafo rápido no tiene memoria: la fracción
  real que recupera el eco es 0.

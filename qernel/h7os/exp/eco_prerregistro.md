# Experimento del eco — prerregistro

Escrito el 2026-10-07, antes de que exista el código que lo corre. La huella
SHA-256 de este archivo se guarda en `exp/eco_prerregistro.sha256` y el README
la cita: si el archivo cambia después de ver resultados, la huella deja de
coincidir. Cualquier desviación se anota en una sección aparte al final de
`exp/eco_resultados.md`; este archivo no se edita.

## Pregunta

¿Sirve el parámetro de orden de Kuramoto, r = |⟨e^{iθ}⟩|, para distinguir
**entrelazamiento con un entorno** de **desfase por un campo clásico**? Si no
sirve para eso, ¿qué distingue en realidad?

## Sistema

- 3 qubits `q[0..2]` en GHZ: `h q[0]; cx q[0],q[1]; cx q[0],q[2];`
  → (|000⟩ + |111⟩)/√2.
- Cualquier ruido de este experimento es diagonal en Z: solo cambia la fase
  relativa θ entre |000⟩ y |111⟩. El estado queda (|000⟩ + e^{iθ}|111⟩)/√2.
- Lectura: ⟨XXX⟩ = cos θ y ⟨YXX⟩ = sin θ, así que por realización
  c = ⟨XXX⟩ + i⟨YXX⟩ = e^{iθ} (con un entorno cuántico, c es la coherencia
  del estado reducido).
  - base XXX: `h q;`
  - base YXX: `sdg q[0]; h q[0]; h q[1]; h q[2];`
  - después `measure q -> c;`.
  El simulador calcula el valor esperado exacto de la paridad ⟨Z⊗3⟩ justo
  antes de esa medición: no hay ruido de disparos, solo el de las realizaciones.
- Estadístico: r = |(1/M) Σ_i c_i| sobre M realizaciones del ruido.
- Tiempo: K = 8 pasos de dt = 1/8 (T = 1).
- Eco de Hahn: `x q;` después del paso 4 y otra vez después del paso 8.
- Lo que se toma de Kuramoto y lo que no:
  - Se toma el parámetro de orden r, la longitud del vector medio de las fases.
  - No hay acoplamiento que sincronice. Aquí nada se sincroniza: las fases se
    dispersan, y el eco las re-enfasa por inversión temporal.

## Modelos

Cada realización i usa su propia semilla, que depende solo de (modelo, i).
- La misma semilla sirve para la versión libre, la versión con eco y las dos
  bases de lectura, así que comparan las mismas realizaciones del ruido.
- Los números al azar salen de xorshift32: u = (x >> 8) / 2^24.

| | modelo | qué hace en cada paso | M |
|---|---|---|---|
| A | campo estático | `rz(Δ_j·dt) q[j]`, con Δ_j = 6u − 3 ∈ [−3, 3) sorteado una vez por realización | 1000 |
| B | patadas Z | `z q[j]` con probabilidad p = 0.02, por qubit y por paso | 1000 |
| C | A + B | el `rz` de A, luego la patada de B | 1000 |
| D | telégrafo lento | `rz(2·s_j·dt) q[j]`; s_j = ±1 sorteado al inicio; tras cada paso (salvo el último) s_j cambia de signo con probabilidad 0.05 | 1000 |
| E | telégrafo rápido | `rz(±3·dt) q[j]`, signo independiente en cada paso | 1000 |
| F | entorno cuántico fijo | entorno `e[0..2]` en \|+⟩ (`h e;` al inicio); `cu1(pi/8) q[j],e[j]` en cada paso | 1 |
| G | entorno que cambia | F, más `h e;` después del paso 4 (con o sin eco) | 1 |

- F y G no tienen azar: el entorno es un registro cuántico de verdad, se
  entrelaza con el sistema y nunca se mide.
- Los ángulos viajan en el QASM como fracciones exactas (p. ej.
  `rz(-3/8)`) o como `pi/8`, de modo que cualquier intérprete reconstruye el
  mismo double.

## Predicciones (formas cerradas)

| | r libre | r eco | clase predicha | de dónde sale |
|---|---|---|---|---|
| A | 1.0e-4 → ≈ 0 | 1 | reversible | (sin 3 / 3)³; el eco cancela un campo constante |
| B | 0.3754 | 0.3754 | irreversible | (1 − 2p)^24; Z conmuta con el eco, así que la patada no se deshace |
| C | 3.9e-5 → ≈ 0 | 0.3754 | mixto | A × B |
| D | 0.0017 → ≈ 0 | 0.5606 | mixto | matriz de transferencia del telégrafo (2 estados, 8 pasos)³ |
| E | 0.1775 | 0.1775 | irreversible | cos(3/8)^24; sin memoria entre mitades |
| F | 0 | 1 | reversible | el entorno queda en \|+⟩ o \|−⟩: ⟨+\|−⟩ = 0; con eco vuelve a S\|+⟩ en ambas ramas |
| G | 1/(2√2) = 0.3536 | 0.3536 | irreversible | el `h` del entorno a mitad de camino rompe la simetría que usa el eco |

Tolerancias:
- F y G (exactos): ±1e-9.
- A–E (muestreados, M = 1000):
  - si la predicción es ≈ 0, pasa con r ≤ 0.15;
  - si no, pasa con |r − predicho| ≤ 0.15.
- A con eco: 1 ± 1e-9 (cancelación exacta en cada realización).
- B: libre = eco exactamente, porque son las mismas realizaciones.

Regla de clasificación (en este orden):
1. **reversible** si r_eco ≥ 0.9 y r_libre ≤ 0.5·r_eco;
2. **irreversible** si |r_eco − r_libre| ≤ 0.1;
3. **mixto** en cualquier otro caso.

Análisis de potencia hecho antes (numpy, 4000 repeticiones del experimento
completo):
- con M = 400, E caería en «mixto» por azar un 3.8 % de las veces; por eso
  M = 1000;
- con M = 1000, la probabilidad estimada de una clase distinta por muestreo es
  ≤ 0.3 % por modelo, y la de salir de la tolerancia es < 0.1 %.

## Hipótesis

- **H-ingenua**: r con y sin eco separa «entorno cuántico (entrelazamiento)»
  de «campo clásico».
  - Operativamente, existiría una regla sobre el par (r_libre, r_eco) que pone
    F y G de un lado y A–E del otro.
- **H-memoria**: r con y sin eco separa según la **memoria del entorno** durante
  el ciclo.
  - Si el entorno no cambia entre las dos mitades, el eco lo deshace
    (reversible).
  - Si cambia más rápido que el eco, no (irreversible).
  - En medio queda mixto.
  - Que el entorno sea cuántico o clásico no entra.

Predicción: H-memoria. Criterio de refutación de H-ingenua, fijado ahora:
- si F queda dentro de la tolerancia de A en los dos números, y
- G queda en la misma clase que B y E,

ninguna regla sobre (r_libre, r_eco) puede separar lo cuántico de lo clásico
en estos modelos, y H-ingenua queda refutada.

Lo que sí mide r en el sistema: el estado promediado es
½(|000⟩⟨000| + |111⟩⟨111|) + coherencia de módulo r/2. Por el criterio de
Gühne–Seevinck (|ρ₁₈| ≤ Σ √(ρ_jj ρ_{9−j,9−j}) para estados biseparables), con
las demás poblaciones en 0, **r > 0 ⟺ entrelazamiento genuino de 3 qubits**.
- r sí mide cuánto entrelazamiento le queda al sistema.
- r no dice por qué lo perdió.

## Qué contaría como error de implementación (no como resultado)

- A con eco ≠ 1 ± 1e-9.
- B libre ≠ B eco.
- F ≠ (0, 1) ± 1e-9.
- G ≠ (0.353553, 0.353553) ± 1e-9.
- El kernel y la herramienta de Linux dan números distintos.
- numpy, corriendo los mismos QASM exportados, da otro r (± 1e-9).
- La Monte Carlo propia de numpy (con su propio generador) se aleja de la forma
  cerrada más que la tolerancia.

## Verificación

1. El kernel (`q echo`) y `tools/echo_exp` (Linux, mismo código C) imprimen
   tablas idénticas byte a byte.
2. `test/echo_ref.py`:
   - corre en numpy todos los circuitos QASM exportados y reconstruye r
     (± 1e-9);
   - hace su propia Monte Carlo con otro generador;
   - evalúa las formas cerradas.
3. El resultado se compara con esta tabla en `exp/eco_resultados.md`, sin
   reescribir las predicciones.

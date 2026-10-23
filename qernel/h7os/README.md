# H7-OS v0.7

Mini sistema operativo experimental para i386. Arranca por multiboot en QEMU,
porta el módulo **H7 Hexa Dec** (`ref/h7_hexa_dec.c`) a C freestanding — sin
libc ni libm — y monta encima un **almacén Z7 de registros de 23 bytes** que se
maneja desde un **shell** por teclado o por la terminal serial. Encima corre un
pequeño laboratorio cuántico: el código de Steane sobre un tablero de
estabilizadores y, desde v0.5, un **vector de estado** con fases arbitrarias, en
el que se hicieron dos experimentos prerregistrados (`exp/`): el eco (v0.5) y la
prueba de unitalidad (v0.6). La v0.7 agrega la fase áurea exacta con enteros
(`phase`), la lectura determinista de n mod 14 en la base GHZ (`q ghz`) y, en el
eco, un análisis de ruido de muestreo separado del prerregistro.

```
make          # compila build/h7os.elf
make run      # QEMU: ventana VGA + el mismo shell en esta terminal
make test     # arranque sin pantalla con un script de comandos y comprobaciones
make iso      # build/h7os.iso: arrancable en VirtualBox, VMware, una USB real...
```

Requisitos en Linux Mint/Ubuntu: `gcc`, `binutils`, `make`, `qemu-system-x86`.
Si `gcc -m32` falla, instala también `gcc-multilib`. `make test` usa además
`python3` con `numpy` para la referencia cuántica (si falta, esa parte se omite).
`make iso` necesita `grub-pc-bin grub-common xorriso`.

## ISO para VirtualBox (u otra máquina virtual)

`make iso` empaqueta el mismo `build/h7os.elf` con GRUB2 en un `.iso` **híbrido
BIOS + EFI** (`boot/grub.cfg` trae el menú; GRUB carga el kernel con el comando
`multiboot`, el mismo protocolo que ya usa QEMU con `-kernel`). El `.iso` también
está adjunto, ya compilado, como `h7os-v0.6.iso` (`make iso` genera el de v0.7). Necesita `grub-pc-bin
grub-common grub-efi-ia32-bin xorriso` para reconstruirse.

En VirtualBox:
1. Nueva máquina → tipo **Other**, versión **Other/Unknown (32-bit)**.
2. Memoria: 64 MB bastan (el almacén vive en RAM y no hay disco).
3. No agregues disco duro.
4. En Almacenamiento, monta `h7os-v0.6.iso` en la unidad óptica.
5. Arranca: el menú de GRUB aparece 3 s y entra directo al shell de H7-OS.
6. **Deja "Enable EFI" apagado** (el valor por defecto) — ver la nota de abajo.

El teclado se lee por PS/2 (posición física US); el puerto serie (COM1) también
está activo, así que si conectas COM1 a un socket o archivo en la configuración
de la VM, el shell responde ahí igual que en `make run`.

**BIOS (modo por defecto): probado y completo.** El ISO se verificó arrancando
como CD-ROM en QEMU (`qemu-system-i386 -cdrom`, sin EFI): mismo banner, misma
autoprueba y pantalla VGA, idéntico a `make run`.

**EFI: arranca, pero sin pantalla VGA.** El ISO también se probó con firmware
UEFI de 32 bits de verdad (OVMF). El kernel arranca, corre su autoprueba y llega
al shell — confirmado por el puerto serie — pero la pantalla se queda en negro:
GRUB avisa `error: no suitable video mode found` porque un firmware UEFI sin
BIOS de VGA heredado no deja el modo texto clásico (80×25 en `0xB8000`) listo
como sí lo hace el BIOS; el driver de video de H7-OS (`con.c`) escribe
directamente ahí. Con EFI encendido, H7-OS sigue funcionando — pero solo por
COM1, no por pantalla. Por eso el menú de GRUB arranca con legado por defecto,
y conviene dejarlo así salvo que solo vayas a usar el puerto serie.

## Shell

Se escribe en la ventana de QEMU (teclado PS/2) o en la terminal donde corriste
`make run` (COM1). Las dos entradas alimentan la misma línea; la salida va a las dos.

```
h7> put 36            guarda n=36 con v = (-1)^n·cos(πφn)
h7> put 36 0.5        o con un valor propio (cuenta updates)
h7> get 36            registro decodificado: topología, valor, clase Z7
h7> flip 36 100       invierte el bit 100 → máscara 0b0000010 → roto: 1
h7> ls                cubos: registros, suma Z7 y sello
h7> verify            máscara de 7 bits y por qué está roto cada cubo
h7> fix               corrige con Hamming los registros con 1 bit dañado
h7> seal              re-sella los cubos íntegros (como un fsck)
h7> sweep 8           todos los flips de 1 bit + todos los pares en n=8, por capa
h7> q run x 3          código de Steane: error X en la posición 3, síndrome, corrección
h7> q qasm x 3         el mismo circuito en OpenQASM 2.0, listo para exportar
h7> q echo             experimento del eco: r libre y con eco en 7 modelos (≈5 s)
h7> q echo F           un modelo, con la fidelidad GHZ (1 + r)/2
h7> q echo qasm D eco  el circuito de una realización, en OpenQASM 2.0
h7> q unital           prueba de unitalidad: z0, z1 y |t| en 6 modelos (≈5 s)
h7> q unital qasm E z  el circuito del entorno frío, lectura Z
h7> phase 1000000000   nodo, posición, etiqueta y cos(πφn) exacto contra rotate_pow
h7> q ghz              n mod 14 en la base GHZ: 14 lecturas con P = 1
h7> q ghz 15           un n: estado, lectura, bit r ≥ 4 y n mod 14
h7> q ghz qasm 15 enc  el circuito; con enc, solo la codificación
h7> del 36 · ref · clear · help · exit · halt
```

- Los enteros aceptan decimal, `0b` binario y `0x` hex: `put 0b101` es `put 5`.
- Flecha arriba repite la última línea; retroceso borra.
- El teclado se lee por posición física de US, pero la tecla del guion del
  teclado latinoamericano también escribe `-`.
- `exit` cierra QEMU con código según el almacén: sano → 1, roto → 3.
- Línea de comandos del kernel: `-append "run=put 36;flip 36 100;ls"` ejecuta
  comandos al arrancar; `vgadump` copia la pantalla al serial al salir.

## Piezas

| archivo | qué es |
|---|---|
| `src/boot.S` | cabecera multiboot, GDT plana propia, pila de 16 KiB, FPU x87 a 53 bits |
| `src/isr.S` | 48 entradas de interrupción → `isr_dispatch` |
| `src/int.c` | IDT, PIC 8259 remapeado a 0x20/0x28, teclado PS/2 (IRQ1) |
| `src/con.c` | VGA + COM1 (IRQ4), cursor, anillo de entrada, `kprintf` con `%f` propio |
| `src/h7.c` | núcleo H7 portado: `n_z7`, `rotate_pow`, topología de 16 bits, registro de 23 B |
| `src/rs.c` | almacén Z7 con índice aparte y Hamming SECDED: `rs_put`, `rs_get`, `rs_del`, `rs_verify`, `rs_ecc`, `rs_fix_slot`, `rs_seal` |
| `src/shell.c` | comandos, edición de línea, script de arranque |
| `src/qsim.c` | tablero de estabilizadores (Aaronson–Gottesman): H, S, CNOT, Paulis, medición |
| `src/qsv.c` | vector de estado (hasta 10 qubits), seno/coseno/raíz propios: mismos bits en x87 y SSE2 |
| `src/qasm.c` | intérprete de OpenQASM 2.0 con dos simuladores (tablero o vector), ángulos como expresiones, `if` |
| `src/steane.c` | genera el circuito de Steane en QASM y lo corre |
| `src/echo.c` | experimento del eco: genera cada realización en QASM, la corre y clasifica |
| `src/unital.c` | prueba de unitalidad: qubit + referencia de Bell, seis modelos de canal |
| `src/ghz.c` | v0.7: n mod 14 codificado en la base GHZ de 3 qubits y leído sin azar |
| `src/qtext.c` | texto y azar comunes a los experimentos (mismos bytes en kernel y Linux) |
| `src/kernel.c` | secuencia de arranque |
| `tools/steane_gen.c` | el mismo generador, compilado para Linux |
| `tools/echo_exp.c` | el experimento del eco en Linux (mismo código): tabla, `raw`, `qasm`, `dump` |
| `tools/unital_exp.c` | la prueba de unitalidad en Linux (mismo código): tabla, `raw`, `qasm`, `dump` |
| `tools/qasm_sv.c` | cualquier QASM por el vector de estado del kernel, en Linux |
| `tools/ghz_exp.c` | `q ghz` en Linux (mismo código): tabla, un `n`, `qasm`, `raw` |
| `tools/phase_tool.c` | `h7_label`, `h7_golden` y `rotate_pow` para 4022 enteros de 64 bits |
| `exp/` | prerregistros (con su SHA-256) y resultados de los dos experimentos |
| `boot/grub.cfg` | menú de GRUB2 para `make iso` (arranca el kernel con `multiboot`) |
| `qasm/steane.qasm` | el circuito sin errores, tal como lo exporta `q qasm` |
| `test/qasm_ref.py` | referencia independiente: vector de estado en numpy |
| `test/qasm_fuzz.py` | circuitos al azar: amplitudes del kernel contra numpy |
| `test/echo_ref.py` | el eco verificado sin el código C: formas cerradas, los mismos QASM en numpy, Monte Carlo propia |
| `test/unital_ref.py` | la unitalidad verificada sin el código C: los mismos QASM en numpy y los canales con matrices de densidad |
| `test/ghz_ref.py` | `q ghz` verificado sin el código C: estado codificado, lectura y n mod 14 en numpy |
| `test/phase_ref.py` | la fase exacta contra `decimal` a 120 dígitos, y las líneas `phase` del kernel |
| `test/test.sh` | script de 47 comandos + comprobaciones + diffs contra las referencias |
| `ref/h7_hexa_dec.c` | tu módulo original, sin tocar |

## El almacén Z7

- **Cubo** = `n mod 7`. El registro se guarda tal cual en formato de cable (`q H d B i`, big endian).
- **Clase Z7 del registro**: los 23 B leídos como un entero de 184 bits, módulo 7.
  Como 2³ ≡ 1 (mod 7), no hace falta dividir: se suman palabras de 24 bits y se
  pliega en octal (`h7_fold7`).
- **Sello** de cada cubo = suma Z7 de sus registros `XOR 111`. Cubo sano ⇔ `suma + sello = 111 ≡ 0`.
- **Índice aparte (v0.2.2)**: qué `n` vive en qué ranura se guarda fuera del
  registro, como una entrada de directorio. Un bit invertido en el campo `n` ya
  no pierde el registro: `get`, `flip` y `del` lo siguen encontrando por su clave,
  y el desacuerdo entre índice y registro cuenta como fallo.
- **Invariantes por registro**: el campo `n` coincide con su clave,
  `index = n mod 7` e `index + pair ≡ 0 (mod 7)`.
- **Sello incremental (nuevo en v0.2)**: `put` y `del` suman la clase del registro
  nuevo y restan la del que sale. Si un cubo ya estaba roto, escribir encima no
  borra la marca: el sello recuerda que algo cambió fuera de las escrituras.
  Solo `seal` acepta el estado actual, y nunca en un cubo con un registro que
  rompe sus invariantes: ese hay que repararlo (`flip` de vuelta), reescribirlo
  (`put`) o borrarlo (`del`).

- **Hamming extendido, SECDED (v0.3)**: 9 bits por registro, guardados junto al
  índice. Ver la sección siguiente.

Resultados medidos:

- Flips de 1 bit: todos detectados y localizados en su cubo, y todos corregidos
  por Hamming (2576/2576 y 2576/2576 al arrancar). Ningún 2ᵏ es múltiplo de 7.
- Flips de 2 bits en `n=8` sin Hamming: 15858/16836 detectados (15838 sin el
  índice aparte: con él, cualquier par que toque el campo `n` se detecta). Los no detectados son
  todos pares a distancia múltiplo de 3 en sentidos opuestos (+2ᵏ y −2ᵏ⁺³ʲ), que
  se cancelan módulo 7, fuera del campo `n`. **Con Hamming: 16836/16836.**

## Hamming extendido (SECDED) por registro

Un registro tiene 184 bits, y entre 1 y 192 hay exactamente 184 números que no
son potencia de 2 (192 − 8). Cada bit de datos `i` recibe una de esas
posiciones `p(i)`; las potencias de 2 (1, 2, 4, …, 128) quedan para los 8 bits
de paridad `P`, y un bit más `q` es la paridad global.

- **Codificar:** `P = XOR de p(i)` sobre los bits encendidos. Así el XOR de las
  posiciones de toda la palabra (datos + P) vale 0. `q` hace par el total de unos.
- **Síndrome:** `s = P ⊕ XOR de p(i)` sobre los bits encendidos ahora.
- **1 bit dañado en i:** el XOR cambia exactamente en `p(i)`, así que `s = p(i)` y
  la paridad global queda impar. Se localiza y se corrige (`fix`).
- **2 bits dañados en i ≠ j:** `s = p(i) ⊕ p(j) ≠ 0` porque las posiciones son
  distintas, y la paridad global queda par. Se detecta siempre; no se corrige.

Es la misma construcción que el código [7,4,3] — columnas = posiciones en
binario, síndrome = posición del error — con 8 bits de síndrome en lugar de 3.
El [7,4,3] es el miembro r = 3 de esa familia; para 184 bits, r = 8 cuesta 9
bits por registro en lugar de los 138 que costarían 46 bloques de [7,4,3].

Las tres capas, de la más barata a la más fuerte:

| capa (acumulada) | costo | 1 bit | pares en n=8 detectados |
|---|---|---|---|
| sello Z7 por cubo | 3 bits por cubo | detecta y localiza el cubo | 14826/16836 |
| + invariantes del registro | nada: se leen del registro | — | 15838/16836 |
| + índice aparte | clave de 8 B por ranura | ve daños en el campo `n` | 15858/16836 |
| + Hamming SECDED | 9 bits por registro | localiza el bit y lo corrige | 16836/16836 |

## Steane [[7,1,3]] en OpenQASM 2.0

Una sola fuente de verdad: `steane.c` escribe el circuito como texto QASM,
`qasm.c` lo interpreta dentro del kernel y `qsim.c` lo ejecuta sobre un tablero
de estabilizadores. `q qasm` imprime exactamente el texto que se ejecutó.

```
h7> q run x 1 x 2
estado |0_L> · errores X en 1, X en 2
paridad Z  sz 011 → x en la posición 3
paridad X  sx 000 → sin error de fase
lectura    m 0100101 (posiciones 1..7) · síndrome clásico 000 · paridad 1
resultado  lógico 1: error lógico (d = 3 solo garantiza corregir 1 error)
```

El circuito (13 qubits: 7 de datos `q`, 6 auxiliares `a`) sale entero de las
tres filas de Hamming — la fila k son las posiciones con el bit k encendido:

1. **Codificar |0_L⟩:** H en las posiciones 1, 2 y 4 (pivotes) y CNOT de cada
   pivote a las demás posiciones de su fila. Queda la superposición uniforme de
   las 8 palabras de peso 0 y 4. Con `+`, una H transversal da |+_L⟩.
2. **Paridades Z** (`sz`, ven errores X): CNOT de los datos de la fila k a `a[k]`.
3. **Paridades X** (`sx`, ven errores Z): `a[3+k]` en |+⟩, CNOT a los datos, H.
4. **Corrección:** `if(sz==j) x q[j-1];` y `if(sx==j) z q[j-1];` — el síndrome es la posición.
5. **Lectura:** medir los 7 datos da una palabra de Hamming; su síndrome clásico
   (XOR de posiciones, como en `rs.c`) debe ser 000 y su paridad es el valor lógico.

Resultados (`q sweep`, 422 circuitos):

- Sin error: síndromes 000/000 y lógico 0 en las dos bases.
- 1 error (X, Y o Z en cualquiera de los 7 qubits): 21/21 corregidos en |0_L⟩ y en |+_L⟩.
- 2 errores: 42/189 sin error lógico. Fallan los pares donde ambos errores
  tienen parte X (o ambos parte Z): con X en i y en j el síndrome es i ⊕ j, la
  corrección pone X en k = i ⊕ j, y X_i X_j X_k es una palabra de peso 3: un X
  lógico. Es el límite de distancia 3, no un defecto del circuito.

**Verificación:** `make test` exporta el QASM desde el kernel y lo corre en
`test/qasm_ref.py`, un simulador de vector de estado en numpy (8192 amplitudes,
sin tablero). Da los mismos síndromes y los mismos conteos del barrido, y
comprueba que las 6 mediciones de síndrome son deterministas en los 422 circuitos.

**Para hardware:** `qasm/steane.qasm` es OpenQASM 2.0 estándar. Las líneas
`if(sz==j)` necesitan un backend con circuitos dinámicos (medición a mitad del
circuito y control clásico). El tablero solo admite puertas de Clifford; los
ángulos arbitrarios van al vector de estado (siguiente sección).

## Experimento del eco (v0.5)

Pregunta: ¿el parámetro de orden de Kuramoto, r = |⟨e^{iθ}⟩|, distingue
**entrelazamiento con un entorno** de **desfase por un campo clásico**?

**Diseño:**
- GHZ de 3 qubits.
- Siete modelos de desfase: cinco campos clásicos (A–E) y dos entornos
  cuánticos de verdad (F, G), con 3 qubits más que se entrelazan y nunca se
  miden.
- Cada uno se corre con y sin eco de Hahn.
- Cada realización es un circuito OpenQASM 2.0 con los ángulos escritos como
  fracciones exactas.
- De ⟨XXX⟩ + i⟨YXX⟩ sale e^{iθ}, y r es el módulo de la media.

Las predicciones, la regla de clasificación y el criterio de refutación se
fijaron antes de escribir el código: `exp/eco_prerregistro.md`, con su SHA-256.

```
  modelo                r libre  r eco    clase        predicha     clase núm
A campo estático        0.0339   1.0000   reversible   reversible   sí    sí
B patadas Z             0.3720   0.3720   irreversible irreversible sí    sí
C estático + patadas    0.0152   0.3540   mixto        mixto        sí    sí
D telégrafo lento       0.0067   0.5599   mixto        mixto        sí    sí
E telégrafo rápido      0.1785   0.2018   irreversible irreversible sí    sí
F entorno cuántico fijo 0.0000   1.0000   reversible   reversible   sí    sí
G entorno que cambia    0.3536   0.3536   irreversible irreversible sí    sí
```

**Resultado: la hipótesis ingenua falla, como se predijo.**
- El entorno cuántico F cae exactamente donde el campo clásico A.
- El entorno cuántico G cae con los ruidos clásicos B y E.
- r con eco no separa cuántico de clásico: separa **memoria del entorno**.
  - Si el entorno no cambia durante el ciclo, el eco lo deshace.
  - Si cambia, no.
  - La familia del telégrafo (A → D → E) lo muestra cambiando solo el tiempo
    de correlación de un mismo ruido clásico.

**No es un accidente numérico.**
- F es un campo clásico disfrazado: `cu1` conserva el valor Z del entorno.
- G se reproduce con un campo que se apaga a mitad de camino.

Lo que sí mide r: con estos estados, r > 0 ⟺ entrelazamiento genuino de los 3
qubits (criterio de Gühne–Seevinck), y la fidelidad con GHZ es (1 + r)/2.

**Siguiente experimento:** salir del desfase puro con una prueba de
unitalidad, que sí separa: ningún campo clásico deja de fijar I/2.

Detalle y desviaciones: `exp/eco_resultados.md`.

**Verificación (`make test`):**
- `q echo` en el kernel = `tools/echo_exp` en Linux, byte a byte;
- numpy corre los 20008 QASM exportados y da los mismos r (|Δ| < 4e-15);
- numpy implementa los modelos por su cuenta y cae a ≤ 0.01 de las formas
  cerradas;
- 300 circuitos al azar con todas las puertas dan amplitudes iguales a 1e-15.

## Prueba de unitalidad (v0.6)

Pregunta: si el desfase no separa lo cuántico de lo clásico, ¿lo separa la
**unitalidad**, es decir, si el canal deja fijo el estado maximalmente mezclado?

Lo que ya es teorema:
- Un campo clásico es una mezcla de unitarios, así que deja I/2 fijo.
- En un qubit vale también al revés: |t| > 0 ⟺ ningún campo clásico explica
  el canal.

**Diseño:**
- Un qubit `q` en par de Bell con una referencia `r`, así que `q` empieza
  exactamente en I/2.
- De un solo circuito salen tres números, porque la referencia recuerda la
  entrada:
  - |t|, el vector de Bloch de Λ(I/2);
  - z0, el ⟨Z⟩ de salida si la entrada fue |0⟩;
  - z1, el ⟨Z⟩ de salida si la entrada fue |1⟩.

```
  modelo            tipo       z de 0   z de 1   |t|     clase      pred   núm
A patadas X         clásico    0.2960  -0.2960   0.0000   unital     sí     sí
B campo 3D al azar  clásico    0.4808  -0.4808   0.0000   unital     sí     sí
C desfase cuántico  cuántico   1.0000  -1.0000   0.0000   unital     sí     sí
D entorno caliente  cuántico   0.2817  -0.2817   0.0000   unital     sí     sí
E entorno frío      cuántico   1.0000   0.4365   0.7183   no unital  sí     sí
F demonio que mide  registro   1.0000   0.4365   0.7183   no unital  sí     sí
```

**Resultado: la unitalidad separa sumideros de entropía, no cuántico de clásico.**
El canal deja de ser unital solo cuando se juntan dos cosas:
1. un entorno con baja entropía (puro, o una memoria en blanco);
2. un acoplamiento que mueve las poblaciones del sistema.

Cómo cae cada modelo:
- C tiene lo primero sin lo segundo;
- D tiene lo segundo sin lo primero;
- E y F tienen las dos.

**La hipótesis ingenua cae otra vez, como se predijo:**
- D, que es cuántico, es unital.
- F es un demonio que hace una medición débil, guarda un bit clásico y corrige.
  Da exactamente el mismo canal que el entorno cuántico frío, incluso en la
  coherencia.

**Por qué:** todo canal admite una versión con registros clásicos, porque cada
descomposición de Kraus es una medición seguida de una corrección.

Juntando los dos experimentos:
- el eco mide **memoria** del entorno;
- la unitalidad mide si el entorno es **sumidero de entropía**;
- ninguno certifica un entorno cuántico.

Para eso hace falta un testigo sobre el tensor de proceso, con varias
intervenciones. Detalle en `exp/unital_resultados.md`.

## v0.7: fase áurea exacta, lectura GHZ y ruido de muestreo del eco

**Fase exacta (`phase n`, `h7_label`, `h7_golden` en `h7.c`).** `rotate_pow`
eleva al cuadrado 63 veces un par (cos πφ, sin πφ) redondeado: el error de fase
crece con n y, además, el módulo se aleja de 1. Contra `decimal` a 120 dígitos
(`test/phase_ref.py`):

| tamaño de n | error de `rotate_pow` |
|---|---|
| 4 dígitos | 3e-12 |
| 7 dígitos | 3e-9 |
| 10 dígitos | 3e-6 |
| 13 dígitos | 3e-3 |
| 16 a 19 dígitos | sin sentido (en n = 2⁶³ − 1 devuelve −1.0e-186) |

`h7_golden` calcula n·φ/2 módulo 1 en punto fijo de 128 bits, solo con
productos enteros 32×32 → 64, y deja el coseno para el final con el argumento
ya reducido: error ≤ 8.9e-16 en 4022 enteros de 64 bits de ambos signos.
`h7_label` es frac(n·φ) con 64 bits; su constante es la del Fibonacci hashing de
Knuth (`0x9E3779B97F4A7C15`). Los registros y `ref` siguen usando `rotate_pow`
para quedar idénticos byte a byte a `h7_hexa_dec.c`; `phase` muestra las dos.

**Lectura GHZ (`q ghz`, `src/ghz.c`).** El estado de 3 qubits de la «Matrioska»,
(|r⟩ + (−1)ⁿ |7−r⟩)/√2 con r = n mod 7, recorre exactamente la base GHZ:
8 estados ortonormales. Medido en Z da r o 7−r al 50 % y pierde el signo;
con CX q2→q0, CX q2→q1 y H q2 antes de medir, la lectura es determinista:

| lectura | 000 | 100 | 101 | 010 | 111 | 011 | 110 | 001 |
|---|---|---|---|---|---|---|---|---|
| n mod 14 | 0 | 7 | 1, 13 | 2, 12 | 3, 11 | 4, 10 | 5, 9 | 6, 8 |

c2 es la paridad de n (el valor propio de XXX) y c1 c0 nombran el par
{r, 7−r}. La lectura junta n con 14 − n; el bit clásico r ≥ 4 dice cuál de los
dos es el residuo y completa n mod 14 (14/14). `test/ghz_ref.py` lo comprueba en
numpy para n = −49..62.

**Eco: ¿r distinto de 0?** Con M = 1000, un estado totalmente desfasado da
|media| ≈ 0.886/√M ≈ 0.028, no 0. `q echo` agrega, después de todo lo
prerregistrado y sin cambiar clases, tolerancias ni criterio, r ± σ (método
delta) y p = exp(−M·r²) (prueba de Rayleigh; en estos modelos da entre 5.0 y
5.5 % de falsos positivos al nivel 0.05). A, C y D libres quedan con p = 0.32,
0.79 y 0.96: no se distinguen de 0, así que su fidelidad GHZ > 1/2 no demuestra
entrelazamiento genuino. Detalle en `exp/eco_resultados.md`.

## Hash QNN de Jako: covarianza exacta (herramienta, no experimento)

`generate_metriplectic_hash()` (`h7_qnn_hash.py` + `h7_bridge.py`, fuera de
este repo) encadena un circuito de 3 qubits para darle contexto a cada
llamada de un modelo local de Ollama. `src/qnnhash.c` corre el mismo circuito
con `qsv.c` y lee la probabilidad exacta (Born) en vez de 512 *shots* de
AerSimulator: mismo circuito, sin ruido de muestreo, reproducible bit a bit
entre el kernel y Linux como el resto de H7-OS. No es un comando del shell,
es una herramienta de Linux para comparar contra el original:

```
qnnhash_tool <n> [iteraciones=3]       reporte legible
qnnhash_tool raw <n> [iteraciones=3]   una línea, para test/qnnhash_ref.py
```

`test/qnnhash_ref.py` aplica a mano, con numpy y sin Qiskit, las mismas
matrices (H, RZ, RY, RX, CCX, CSWAP), y recalcula `n_z7`/`o_n`/`i_n` con
`math.cos` puro de Python — así también vigila que `rotate_pow` (`h7.c`) no
se haya desviado.

De las cantidades que expone `generate_metriplectic_hash`:
- `covariance`/`asymmetry` sí son nuevas en cada semilla; `asymmetry` resulta
  ser exactamente `E(q2) − E(q1)` (el término `E(q1q2)` se cancela).
- `psi` y `energy` (`= o_n·i_n + i_n`) dependen solo de `o_n = cos(πn)` e
  `i_n = rotate_pow(n) = cos(πφn)` — las mismas dos cantidades de `h7.c`.
  `energy` es **0 exacto en toda semilla final impar** (ahí `o_n = −1` anula
  el factor `o_n + 1`) y vale `2·i_n` en las pares.
- `torsion` (`spatial_torsion` en `h7_bridge.py`) es la constante `2π − 7`:
  no depende de `n`, del estado ni de nada que entre a la función.
- `su2_original` se calcula en `run_qnn_step` pero nunca se aplica al
  circuito; no afecta ninguno de los números anteriores, así que
  `qnnhash.c` no lo porta (tampoco el cuaternión, `QNNGrid` ni la
  exportación a JSON/binario — ninguno alimenta `psi`/`energy`/`torsion`).

## Qué es estructura y qué es nombre

- `h7_bridge.py` (hash QNN de Jako): `energy`, `energy_density`, `chirality`
  y `v` son **nivel 0** — el mismo par `(o_n, i_n)` repetido con otro nombre
  cada vez. `spatial_torsion` ni llega a nivel 1: es una constante (`2π−7`),
  no una cantidad derivada. El cuaternión de `su2_to_cuaternion` sí es nivel
  3 (SU(2) y los cuaterniones unitarios son isomorfos de verdad) — pero no
  entra en el hash final.

- Kuramoto en el experimento del eco:
  - El parámetro de orden r es **nivel 3**: el mismo objeto, con el mismo
    significado (coherencia de un conjunto de fases).
  - Kuramoto sin acoplamiento (K = 0) también es nivel 3: es exactamente el
    modelo A (dθ/dt = ω_j, desintonías repartidas, mismas unidades).
  - El acoplamiento K no tiene contraparte aquí.
  - La frontera cuántico/clásico no la captura r.

- `uud`, `VACUUM`, `TOROID`, *metripléctico*: **nivel 1**, nombres para patrones de 3 bits.
- `n mod 7` con plegado octal, el complemento `XOR 111` y el doble cero `000`/`111`
  son exactamente la aritmética en complemento a uno de 3 bits, el mismo mecanismo
  del checksum de Internet (RFC 1071, módulo 2¹⁶−1).

## Cambios respecto a `h7_hexa_dec.c`

- `complement_block` devuelve `int64_t`; el original truncaba a `int` para |n| > 2³¹.
- `cos(πφ)` y `sin(πφ)` son constantes hex-float con los mismos bits que glibc.
- `htobe*` → `__builtin_bswap*`; `round`/`fabs` → versiones exactas sin libm.
- En el original, `printf("%4ld", n)` con `int64_t` solo es correcto en x86-64; lo portable es `PRId64`.
- v0.7: `rotate_pow` toma |n| como `uint64_t`. En el original (y en v0.6), `-n` con
  n = INT64_MIN desborda, y con `-O2` el ciclo no termina. Para cualquier otro n
  los bits son los mismos. `ref/h7_hexa_dec.c` se deja como estaba.

## Límites

Sin temporizador (IRQ0), paginación ni disco: el almacén vive en `.bss` y se
pierde al apagar. `n` en el shell es de 32 bits (el registro guarda 64) para no
depender de la división de 64 bits de libgcc. `rotate_pow` pierde precisión
con n grande (ver v0.7); con los n de 32 bits del shell el error llega a 7.6e-7. Siguiente paso natural: persistir
la arena en un disco ATA en bloques de 23 B y leerla al arrancar.

#!/bin/sh
# test.sh — arranca H7-OS sin pantalla, le pasa un script de comandos por la
# línea de comandos del kernel y comprueba la salida del serial.
set -u
cd "$(dirname "$0")/.."

SCRIPT="ref;ls;put 36;get 36;flip 8 100;verify;flip 8 100;verify;put 8;get 8;flip 15 100;put 15;verify;seal;verify;del 22;put 0b101;get 0x5;flip 19 2;get 19;seal;flip 19 2;flip 8 100;fix;flip 8 100;flip 8 3;fix;flip 8 3;flip 8 100;sweep 8;q run x 3;q run + z 5;q run y 7;q run x 1 x 2;q qasm;q qasm x 3;q sweep;q echo;q echo qasm F eco;q unital;q unital qasm E z;phase 1000000000;phase -15;q ghz;q ghz 15;q ghz qasm 15;exit"

timeout 120 qemu-system-i386 -kernel build/h7os.elf -append "vgadump run=$SCRIPT" \
    -display none -serial file:build/serial.log -device isa-debug-exit,iobase=0xf4,iosize=0x04
s=$?
tr -d '\r' < build/serial.log > build/boot.log

fail=0
check() {   # check "descripción" "texto exacto esperado"
    if grep -qF -- "$2" build/boot.log; then echo "ok    $1"; else echo "FALLO $1: falta «$2»"; fail=1; fi
}

[ $s -eq 1 ] && echo "ok    exit con almacén sano (QEMU devolvió 1)" || { echo "FALLO exit: QEMU devolvió $s"; fail=1; }

./build/ref > build/ref.txt
sed -n '/^--8<-- ref$/,/^-->8-- ref$/{//!p}' build/boot.log > build/kernel_ref.txt
if diff -q build/ref.txt build/kernel_ref.txt >/dev/null; then
    echo "ok    ref idéntico byte a byte a h7_hexa_dec.c"
else
    echo "FALLO ref difiere de h7_hexa_dec.c"; diff build/ref.txt build/kernel_ref.txt | head; fail=1
fi

check "autotest al arrancar"        "1 bit: 2576/2576 detectados en su cubo · 2576/2576 corregidos"
check "put de un n nuevo"           "n=36 → cubo 1 (001) · updates 0"
check "flip detectado en cubo 1"    "n=8 bit 100 (byte 12, d · valor) · máscara 0b0000010 → roto: 1"
check "flip de vuelta repara"       "n=8 bit 100 (byte 12, d · valor) · máscara 0b0000000 → 7/7 cubos sanos"
check "put repetido cuenta updates" "n=8 → cubo 1 (001) · updates 1"
if grep -A4 -F 'h7> put 15' build/boot.log | grep -qF 'máscara 0b0000010 → roto: 1'; then
    echo "ok    escribir encima no lava un fallo"
else
    echo "FALLO escribir encima borró la marca del fallo"; fail=1
fi
check "seal acepta el estado"       "sellos recalculados · máscara 0b0000000 → 7/7 cubos sanos"
check "del"                         "n=22 borrado · máscara 0b0000000"
check "0b101 y 0x5 son n=5"         "n=5 · z 5 (101) · dud"
check "flip en el campo n no pierde el registro" "ojo: el campo n vale 0x2000000000000013; el índice lo encontró por su clave 19"
check "seal no acepta un registro dañado" "no se re-sellan los cubos con registros dañados: 5"
check "flip de vuelta en el campo n repara" "n=19 bit 2 (byte 0, q · n) · máscara 0b0000000 → 7/7 cubos sanos"
check "fix corrige 1 bit"           "n=8: bit 100 corregido (síndrome 01101100 = posición 108, byte 12, d · valor)"
check "fix rechaza 2 bits"          "n=8: no se puede corregir (2 o más bits)"
check "sweep 1 bit en 15 registros" "1 bit:  2760/2760 detectados en su cubo · 2760/2760 corregidos por Hamming"
check "sweep 2 bits en n=8"         "2 bits en n=8: sin Hamming 15843/16836 · con Hamming 16836/16836"

# --- Steane en OpenQASM ---------------------------------------------------
awk '/^--8<-- qasm$/{n++; f="build/kernel_qasm" n ".qasm"; next} /^-->8-- qasm$/{f=""; next} f{print > f}' build/boot.log
if diff -q qasm/steane.qasm build/kernel_qasm1.qasm >/dev/null; then
    echo "ok    q qasm idéntico a qasm/steane.qasm"
else
    echo "FALLO q qasm difiere de qasm/steane.qasm"; fail=1
fi
check "Steane: X en 3"              "paridad Z  sz 011 → x en la posición 3"
check "Steane: Z en 5 sobre |+_L>"  "paridad X  sx 101 → z en la posición 5"
check "Steane: Y en 7"              "paridad X  sx 111 → z en la posición 7"
check "Steane: 2 errores X"         "resultado  lógico 1: error lógico (d = 3 solo garantiza corregir 1 error)"
check "q sweep 1 error"             "1 error:   21/21 corregidos en |0_L> · 21/21 en |+_L>"
check "q sweep 2 errores"           "2 errores: 42/189 sin error lógico · 147/189 con error lógico (d = 3)"

if python3 -c 'import numpy' 2>/dev/null; then
    ref=$(python3 test/qasm_ref.py build/kernel_qasm2.qasm)
    case "$ref" in
        "sz 011 · sx 000 · síndrome clásico 000 · paridad 0 · síndromes deterministas: sí")
            echo "ok    numpy corre el QASM exportado y da el mismo síndrome";;
        *)  echo "FALLO numpy con el QASM exportado: $ref"; fail=1;;
    esac
    grep -E '^(sin error: síndromes|1 error: |2 errores: )' build/boot.log > build/q_sweep_kernel.txt
    python3 test/qasm_ref.py --sweep build/steane_gen > build/q_sweep_numpy.txt 2>build/q_sweep_numpy.err
    if diff -q build/q_sweep_kernel.txt build/q_sweep_numpy.txt >/dev/null; then
        echo "ok    q sweep: tablero del kernel = vector de estado en numpy ($(cat build/q_sweep_numpy.err))"
    else
        echo "FALLO q sweep difiere de numpy"; diff build/q_sweep_kernel.txt build/q_sweep_numpy.txt; fail=1
    fi
else
    echo "omitido: referencia numpy (python3 sin numpy)"
fi

# --- experimento del eco (exp/eco_prerregistro.md) -------------------------
if sha256sum -c --status exp/eco_prerregistro.sha256; then
    echo "ok    prerregistro sin cambios desde que se fijó (SHA-256)"
else
    echo "FALLO exp/eco_prerregistro.md cambió después de fijarse"; fail=1
fi
sed -n '/^--8<-- echo$/,/^-->8-- echo$/{//!p}' build/boot.log > build/kernel_echo.txt
./build/echo_exp > build/echo_host.txt
if diff -q build/echo_host.txt build/kernel_echo.txt >/dev/null; then
    echo "ok    q echo idéntico a tools/echo_exp (mismo código en Linux)"
else
    echo "FALLO q echo difiere de tools/echo_exp"; diff build/echo_host.txt build/kernel_echo.txt; fail=1
fi
./build/echo_exp qasm F eco xxx > build/echo_F.qasm
if diff -q build/echo_F.qasm build/kernel_qasm3.qasm >/dev/null; then
    echo "ok    q echo qasm F eco idéntico a echo_exp"
else
    echo "FALLO q echo qasm F eco difiere de echo_exp"; fail=1
fi
check "eco: clases y números"       "clases como se predijo: 7/7 · números en tolerancia: 7/7"
check "eco: comprobaciones exactas" "exactas: A eco = 1 · B libre = eco · F = (0, 1) · G = (0.3536, 0.3536): sí"
check "eco: criterio prerregistrado" "H-ingenua (r separa cuántico de clásico): refutada · F cae con A; G con B y E"
check "eco: análisis posterior separado del prerregistro" "-- después del prerregistro: ¿r distinto de 0 o solo ruido de muestreo? --"
if python3 -c 'import numpy' 2>/dev/null; then
    if out=$(python3 test/qasm_fuzz.py build/qasm_sv 300); then echo "ok    $out"
    else echo "FALLO vector de estado del kernel ≠ numpy"; echo "$out"; fail=1; fi
    python3 test/echo_ref.py build/echo_exp || fail=1
fi

# --- prueba de unitalidad (exp/unital_prerregistro.md) ----------------------
if sha256sum -c --status exp/unital_prerregistro.sha256; then
    echo "ok    prerregistro de unitalidad sin cambios (SHA-256)"
else
    echo "FALLO exp/unital_prerregistro.md cambió después de fijarse"; fail=1
fi
sed -n '/^--8<-- unital$/,/^-->8-- unital$/{//!p}' build/boot.log > build/kernel_unital.txt
./build/unital_exp > build/unital_host.txt
if diff -q build/unital_host.txt build/kernel_unital.txt >/dev/null; then
    echo "ok    q unital idéntico a tools/unital_exp"
else
    echo "FALLO q unital difiere de tools/unital_exp"; diff build/unital_host.txt build/kernel_unital.txt; fail=1
fi
./build/unital_exp qasm E z > build/unital_E.qasm
if diff -q build/unital_E.qasm build/kernel_qasm4.qasm >/dev/null; then
    echo "ok    q unital qasm E z idéntico a unital_exp"
else
    echo "FALLO q unital qasm E z difiere de unital_exp"; fail=1
fi
check "unital: clases y números"    "clases como se predijo: 6/6 · números en tolerancia: 6/6"
check "unital: comprobaciones exactas" "exactas: t_x = t_y = 0 · |t| = 0 en cada realización de A, B, C · E = F: sí"
check "unital: criterio prerregistrado" "H-ingenua-2 (no unital = entorno cuántico): refutada · D es unital y F = E"
if python3 -c 'import numpy' 2>/dev/null; then
    python3 test/unital_ref.py build/unital_exp || fail=1
fi

# --- v0.7: fase áurea exacta (phase) -------------------------------------------
check "phase: nodo y posición de n < 0" "n=-15 · nodo -3 · posición 6 (110) · etiqueta q"
python3 test/phase_ref.py build/phase_tool build/boot.log || fail=1

# --- v0.7: lectura GHZ de n mod 14 (q ghz) --------------------------------------
sed -n '/^--8<-- ghz$/,/^-->8-- ghz$/{//!p}' build/boot.log > build/kernel_ghz.txt
./build/ghz_exp > build/ghz_host.txt
if diff -q build/ghz_host.txt build/kernel_ghz.txt >/dev/null; then
    echo "ok    q ghz idéntico a tools/ghz_exp (mismo código en Linux)"
else
    echo "FALLO q ghz difiere de tools/ghz_exp"; diff build/ghz_host.txt build/kernel_ghz.txt; fail=1
fi
./build/ghz_exp qasm 15 > build/ghz_15.qasm
if diff -q build/ghz_15.qasm build/kernel_qasm5.qasm >/dev/null; then
    echo "ok    q ghz qasm 15 idéntico a ghz_exp"
else
    echo "FALLO q ghz qasm 15 difiere de ghz_exp"; fail=1
fi
check "ghz: lectura determinista"   "estado = (|r> + (−1)^n |7−r>)/√2 en 14/14 · lectura determinista (P = 1) en 14/14"
check "ghz: n mod 14 completo"      "con el bit r ≥ 4: n mod 14 recuperado en 14/14"
check "ghz: n = 15"                 "bit r ≥ 4  0 → n mod 14 = 1 (correcto)"
if python3 -c 'import numpy' 2>/dev/null; then
    python3 test/ghz_ref.py build/ghz_exp || fail=1
fi

# --- hash QNN de Jako (src/qnnhash.c): covarianza/asimetría exactas,
#     sin Aer ni *shots* — no es un comando del shell, solo herramienta Linux
if python3 -c 'import numpy' 2>/dev/null; then
    python3 test/qnnhash_ref.py build/qnnhash_tool || fail=1
fi

grep -v '^vga|' build/boot.log > build/session.log
grep '^vga|' build/boot.log | sed 's/^vga|//' > build/vga.txt

if [ $fail -eq 0 ]; then echo "OK: todas las comprobaciones pasan (sesión completa en build/session.log)"; fi
exit $fail

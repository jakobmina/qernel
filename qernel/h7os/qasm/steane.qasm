OPENQASM 2.0;
include "qelib1.inc";
// H7-OS - codigo de Steane [[7,1,3]] sobre Hamming [7,4,3]
// q[j-1] es la posicion j = 1..7; un sindrome leido en binario es la posicion
qreg q[7];
qreg a[6];
creg sz[3];
creg sx[3];
creg m[7];
// codificar |0_L>: pivotes en las posiciones 1, 2 y 4
h q[0];
h q[1];
h q[3];
cx q[0],q[2];
cx q[0],q[4];
cx q[0],q[6];
cx q[1],q[2];
cx q[1],q[5];
cx q[1],q[6];
cx q[3],q[4];
cx q[3],q[5];
cx q[3],q[6];
barrier q;
// errores inyectados
barrier q;
// paridades Z: sz[k] = posiciones con el bit k encendido (ven errores X)
cx q[0],a[0];
cx q[2],a[0];
cx q[4],a[0];
cx q[6],a[0];
cx q[1],a[1];
cx q[2],a[1];
cx q[5],a[1];
cx q[6],a[1];
cx q[3],a[2];
cx q[4],a[2];
cx q[5],a[2];
cx q[6],a[2];
measure a[0] -> sz[0];
measure a[1] -> sz[1];
measure a[2] -> sz[2];
// paridades X: las mismas posiciones (ven errores Z)
h a[3];
h a[4];
h a[5];
cx a[3],q[0];
cx a[3],q[2];
cx a[3],q[4];
cx a[3],q[6];
cx a[4],q[1];
cx a[4],q[2];
cx a[4],q[5];
cx a[4],q[6];
cx a[5],q[3];
cx a[5],q[4];
cx a[5],q[5];
cx a[5],q[6];
h a[3];
h a[4];
h a[5];
measure a[3] -> sx[0];
measure a[4] -> sx[1];
measure a[5] -> sx[2];
// correccion: el sindrome es la posicion danada
if(sz==1) x q[0];
if(sz==2) x q[1];
if(sz==3) x q[2];
if(sz==4) x q[3];
if(sz==5) x q[4];
if(sz==6) x q[5];
if(sz==7) x q[6];
if(sx==1) z q[0];
if(sx==2) z q[1];
if(sx==3) z q[2];
if(sx==4) z q[3];
if(sx==5) z q[4];
if(sx==6) z q[5];
if(sx==7) z q[6];
// lectura: m es una palabra de Hamming; su paridad es el valor logico
measure q -> m;

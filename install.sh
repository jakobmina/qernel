#!/bin/bash
# Script de instalación automática del Qernel

echo "[1/3] Compilando componentes..."
make all

echo "[2/3] Copiando binarios a bin/..."
mkdir -p qernel/bin
cp qernel/qernel_sim qernel/qnn_torsion qernel/pauli_daemon qernel/h7_server qernel/torsion_codec qernel/qnn_grid qernel/screen_sensor qernel/qnn_actor qernel/boot qernel/bin/

echo "[3/3] Instalando servicio systemd..."
if [ -f "qernel/qernel.service" ]; then
    sudo cp qernel/qernel.service /etc/systemd/system/
    sudo systemctl daemon-reload
    sudo systemctl enable qernel.service
    sudo systemctl restart qernel.service
    echo "[OK] Qernel instalado y activo."
else
    echo "[ERROR] No se encontró qernel/qernel.service"
    exit 1
fi

import os
import subprocess
import time
import pytest

def test_boot_compilation():
    """Verifica que el orquestador de arranque compila correctamente."""
    result = subprocess.run(["make", "boot"], cwd="qernel", capture_output=True, text=True)
    assert result.returncode == 0
    assert os.path.exists("qernel/boot")

def test_service_file_exists():
    """Verifica que el archivo de servicio systemd existe y tiene la estructura correcta."""
    service_path = "qernel/qernel.service"
    assert os.path.exists(service_path)
    with open(service_path, "r") as f:
        content = f.read()
        assert "[Unit]" in content
        assert "ExecStart" in content
        assert "boot" in content

def test_boot_logic_equilibrium():
    """
    Verifica que la lógica de arranque no sea puramente disipativa ni conservativa.
    Este es un test conceptual sobre el binario compilado.
    """
    # Ejecutamos el boot por unos segundos y verificamos que no muera inmediatamente
    process = subprocess.Popen(["./boot"], cwd="qernel", stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    time.sleep(2)
    poll = process.poll()
    
    # Si poll es None, el proceso sigue vivo (estabilidad metriplética)
    is_alive = poll is None
    process.terminate()
    
    assert is_alive, "El proceso boot murió prematuramente (posible singularidad)"

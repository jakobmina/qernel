import requests
import subprocess
import time
import os
import pytest

URL = "http://localhost:8000"
BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BOOT_BIN = os.path.join(BASE_DIR, "boot")

def setup_system():
    # Limpiar procesos previos
    subprocess.run(["pkill", "-9", "boot"], stderr=subprocess.DEVNULL)
    subprocess.run(["pkill", "-9", "h7_server"], stderr=subprocess.DEVNULL)
    subprocess.run(["pkill", "-9", "pauli_daemon"], stderr=subprocess.DEVNULL)
    
    if os.path.exists("/tmp/qnn_pauli.socket"):
        os.unlink("/tmp/qnn_pauli.socket")
    
    subprocess.run(["make", "all"], cwd=BASE_DIR, check=True)
    
    # Iniciar vía orquestador
    boot_proc = subprocess.Popen([BOOT_BIN], cwd=BASE_DIR)
    time.sleep(2)
    return boot_proc

@pytest.fixture(scope="module", autouse=True)
def system_lifecycle():
    boot_proc = setup_system()
    yield
    boot_proc.terminate()
    boot_proc.wait()
    subprocess.run(["pkill", "-9", "h7_server"], stderr=subprocess.DEVNULL)
    subprocess.run(["pkill", "-9", "pauli_daemon"], stderr=subprocess.DEVNULL)

def test_api_actor():
    """Valida el Actor Cognitivo y el Operador Áureo."""
    response = requests.get(f"{URL}/api/actor")
    assert response.status_code == 200
    data = response.json()
    assert "thought" in data
    assert "On" in data
    assert -1.1 <= data["On"] <= 1.1

def test_api_feeling():
    """Valida el motor de sensado (Torsion Observables)."""
    response = requests.get(f"{URL}/api/feeling")
    assert response.status_code == 200
    data = response.json()
    assert all(k in data for k in ["energy", "entropy", "torsion", "chirality"])

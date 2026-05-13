import socket
import struct
import subprocess
import os
import time
import pytest

# Ruta absoluta para evitar errores de directorio
BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BINARY = os.path.join(BASE_DIR, "pauli_daemon")
SOCKET_PATH = "/tmp/qnn_pauli.socket"

def setup_daemon():
    """Lanza el daemon en modo monitoreado."""
    if os.path.exists(SOCKET_PATH):
        os.unlink(SOCKET_PATH)
    
    # Asegurar compilación
    subprocess.run(["make", "pauli_daemon"], cwd=BASE_DIR, check=True)
    
    proc = subprocess.Popen([BINARY], cwd=BASE_DIR)
    
    timeout = 5
    start_time = time.time()
    while not os.path.exists(SOCKET_PATH):
        if time.time() - start_time > timeout:
            proc.terminate()
            raise TimeoutError(f"El daemon no creó el socket en {SOCKET_PATH}")
        time.sleep(0.1)
    return proc

@pytest.fixture(scope="module", autouse=True)
def daemon_lifecycle():
    proc = setup_daemon()
    yield
    proc.terminate()
    proc.wait()
    if os.path.exists(SOCKET_PATH):
        os.unlink(SOCKET_PATH)

def send_to_daemon(n):
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as client:
        client.connect(SOCKET_PATH)
        client.sendall(struct.pack("i", n))
        data = client.recv(1)
        return struct.unpack("B", data)[0]

def test_pauli_logic():
    """Valida la Regla 1.2: Exclusión de Pauli vía GCD(7)."""
    assert send_to_daemon(7) == 0x07
    assert send_to_daemon(14) == 0x07
    assert send_to_daemon(1) == 0x01
    assert send_to_daemon(8) == 0x01

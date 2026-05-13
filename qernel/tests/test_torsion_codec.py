import subprocess
import os
import pytest

BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CODEC_BIN = os.path.join(BASE_DIR, "torsion_codec")

def test_codec_compilation():
    subprocess.run(["make", "torsion_codec"], cwd=BASE_DIR, check=True)
    assert os.path.exists(CODEC_BIN)

def test_ascii_reconstruction():
    """Prueba la codificación/decodificación de un carácter."""
    # El codec toma un char y lo devuelve tras pasar por el espacio de Hilbert
    test_char = "A"
    result = subprocess.run([CODEC_BIN, test_char], cwd=BASE_DIR, capture_output=True, text=True)
    assert result.returncode == 0
    # Verificamos que la salida contenga el carácter original o sea coherente
    assert test_char in result.stdout

def test_chirality_distinction():
    """Verifica que caracteres opuestos generen estados distintos."""
    res1 = subprocess.run([CODEC_BIN, "a"], cwd=BASE_DIR, capture_output=True, text=True)
    res2 = subprocess.run([CODEC_BIN, "A"], cwd=BASE_DIR, capture_output=True, text=True)
    assert res1.stdout != res2.stdout

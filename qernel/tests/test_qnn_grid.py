import subprocess
import os
import pytest

def test_grid_compilation():
    result = subprocess.run(["make", "qnn_grid"], check=True)
    assert os.path.exists("qnn_grid")

def test_grid_layer_mapping():
    """Verifica que el binario reporte las capas correctas para cada par."""
    result = subprocess.run(["./qnn_grid"], capture_output=True, text=True)
    assert result.returncode == 0
    
    output = result.stdout
    # Verificar mapeo de pares
    assert "Capa 0 (Par 0-7)" in output
    assert "Capa 1 (Par 1-6)" in output
    assert "Capa 2 (Par 2-5)" in output
    assert "Capa 3 (Par 3-4)" in output

def test_singularities_clipping():
    """Verifica que no haya valores negativos en las amplitudes (Regla 1.3)."""
    result = subprocess.run(["./qnn_grid"], capture_output=True, text=True)
    import re
    # Buscar todos los números después de psi[x]=
    values = re.findall(r"psi\[\d+\]=([-+]?\d*\.\d+|\d+)", result.stdout)
    for v in values:
        val = float(v)
        assert val >= 0.0, f"Amplitud negativa detectada: {val}"

if __name__ == "__main__":
    pytest.main([__file__])

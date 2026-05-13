import subprocess
import os
import pytest

def test_compilation():
    """Verifica que el sistema compila sin errores."""
    result = subprocess.run(["make", "all"], capture_output=True, text=True)
    assert result.returncode == 0
    assert os.path.exists("qernel_sim")

def test_execution_stability():
    """Verifica que el sistema no explote numéricamente (Regla 1.3)."""
    result = subprocess.run(["./qernel_sim"], capture_output=True, text=True)
    assert result.returncode == 0
    
    lines = result.stdout.strip().split("\n")
    # Ignorar la cabecera
    data_lines = lines[1:]
    
    for line in data_lines:
        parts = line.split(",")
        # Check for NaN or Inf in Psi, H, S
        for val in parts[2:5]:
            f_val = float(val)
            assert not (f_val != f_val), f"NaN detectado en: {line}" # NaN check
            assert abs(f_val) != float('inf'), f"Singularidad (Inf) detectada en: {line}"

def test_source_compliance():
    """Verifica que el código fuente contenga el Lagrangiano obligatorio (Regla 3.1)."""
    with open("src/circuit.c", "r") as f:
        content = f.read()
        assert "compute_lagrangian" in content
        assert "L.H =" in content
        assert "L.S =" in content

if __name__ == "__main__":
    pytest.main([__file__])

import subprocess
import os
import pytest

def test_qnn_compilation():
    result = subprocess.run(["make", "qnn_torsion"], capture_output=True, text=True)
    assert result.returncode == 0
    assert os.path.exists("qnn_torsion")

def test_pauli_exclusion():
    """Verifica que el binario reporte el cumplimiento de Pauli."""
    result = subprocess.run(["./qnn_torsion"], capture_output=True, text=True)
    assert "cumplen Pauli (XOR=7)" in result.stdout

def test_quaternion_math():
    """Valida que el producto de Hamilton sea consistente."""
    result = subprocess.run(["./qnn_torsion"], capture_output=True, text=True)
    # Buscamos q_final.w (Covarianza)
    # Para q_a = {0.0, 0.362, 0.730, 0.500} y q_b = {0.0, -x, -y, -z}
    # w_res = w1*w2 - x1*x2 - y1*y2 - z1*z2 = 0 - (0.362*-0.362) - ...
    # w_res = x^2 + y^2 + z^2
    import math
    expected_w = 0.362**2 + 0.730**2 + 0.500**2
    
    for line in result.stdout.split("\n"):
        if "q_final =" in line:
            # Extraer el primer valor (w)
            actual_w = float(line.split("=")[1].split("+")[0].strip())
            assert math.isclose(actual_w, expected_w, rel_tol=1e-3)

if __name__ == "__main__":
    pytest.main([__file__])

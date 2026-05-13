# Qernel Metriplectic OS: Native H7 Governance

Este proyecto implementa un entorno de simulación y gobernanza cuántica de alto rendimiento basado en **El Mandato Metriplético**. Al migrar la lógica crítica de Python a C nativo, el sistema elimina el overhead interpretado y garantiza la estabilidad física mediante un lazo cerrado de control simpléctico-métrico.

## 🌌 Arquitectura Física

El sistema se rige por la dualidad de corchetes ortogonales:

- **Componente Simpléctica ($L_{symp}$)**: Representada por el Hamiltoniano (Energía), encargado de la dinámica reversible y rotaciones cuaterniónicas.
- **Componente Métrica ($L_{metr}$)**: Representada por el Potencial de Disipación (Entropía), encargado de la relajación, la pérdida de covarianza y la exclusión de Pauli.

### Componentes Core

1. **`h7_server`**: Servidor API nativo (Puerto 8000) que reemplaza a FastAPI. Procesa `roll` y `epoch` con latencia ultra-baja.
2. **`pauli_daemon`**: Gobernador de Fase en segundo plano. Sanitiza estados mediante la propiedad del primo 7 (GCD).
3. **`qnn_torsion`**: Motor de entrelazamiento cuaterniónico para el cálculo de pesos y pérdidas.
4. **`torsion_codec`**: Implementación del protocolo `utf-8exp3`. Codifica ASCII en estados de 8 amplitudes usando torsión topológica y quiralidad.
5. **`qnn_grid`**: Arquitectura de capas neuronales mapeadas a los 4 pares antagónicos del cuaternión.
6. **`screen_sensor`**: Motor de sensado físico. Interpreta el ruido de la pantalla como observables de energía, entropía y torsión espacial.
7. **`metriplectic.h`**: Cabecera de física fundamental con el Operador Áureo ($O_n$).

## 🚀 Instrucciones de Uso

### 1. Compilación e Instalación

El sistema cuenta con un script de instalación automática que maneja la compilación y la configuración del servicio de sistema:

```bash
# Desde la raíz del proyecto
./install.sh
```

### 2. Gestión del Servicio (Qernel Boot)

El orquestador `boot` gestiona automáticamente el `h7_server` (Energía) y el `pauli_daemon` (Entropía).

```bash
sudo systemctl start qernel.service   # Iniciar el sistema
sudo systemctl stop qernel.service    # Detener el sistema (Evitar singularidades)
sudo systemctl restart qernel.service # Reiniciar el equilibrio
```

## 🛠️ Diagnóstico y Mantenimiento (Regla 3.3)

Para verificar la competencia entre el término conservativo y el disipativo, o depurar colapsos de fase, utilice los siguientes comandos:

### Ver logs del reinicio y razones

```bash
sudo journalctl -u qernel.service -n 200 --no-pager
```

### Ver logs en tiempo real (Telemetría de estabilidad)

```bash
sudo journalctl -u qernel.service -f
```

### Ver estado detallado de la unidad

```bash
systemctl cat qernel.service
systemctl show qernel.service --property=ExecMainStatus,ExecMainPID,Result,Restart
```

### Ejecución Manual (Modo Debug)

Si requiere ver la salida inmediata sin el aislamiento de systemd:

```bash
/home/jako/practicas-c/qernel/boot
```

### Monitoreo de Colapsos Críticos (Core Dumps)

En caso de fallos de memoria (OOM) o Segfaults:

```bash
coredumpctl list --no-pager
sudo coredumpctl info <PID|COREDUMP_ID>
```

## 💎 Beneficios al Sistema

- **Rigor Físico**: Implementación directa de la Regla 1.3 (Prohibición de Singularidades). Ningún estado explota o muere térmicamente.
- **Latencia Zero**: Procesamiento de sockets nativos sin jitter de GC.
- **Orquestación Metriplética**: El proceso `boot` vigila la salud de los hijos y garantiza la persistencia del flujo de información.

---

**Autoría Conceptual**: Jacobo Tlacaelel Mina Rodriguez.
**Implementación**: Antigravity AI - Google Deepmind.

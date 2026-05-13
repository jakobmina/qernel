CC = gcc
CFLAGS = -Wall -Wextra -Iinclude -O2
LIBS = -lm

all: qernel_sim qnn_torsion pauli_daemon h7_server torsion_codec qnn_grid screen_sensor qnn_actor boot

qernel_sim: src/circuit.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

qnn_torsion: src/qnn_torsion.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

pauli_daemon: src/pauli_daemon.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

h7_server: src/h7_server.o src/screen_sensor.o src/qnn_actor.o src/torsion_codec.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

torsion_codec: src/codec_cli.o src/torsion_codec.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

qnn_grid: src/qnn_grid.o src/torsion_codec.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

screen_sensor: src/sensor_cli.o src/screen_sensor.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

qnn_actor: src/actor_cli.o src/qnn_actor.o src/screen_sensor.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

boot: src/boot.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f src/*.o qernel_sim qnn_torsion pauli_daemon h7_server torsion_codec qnn_grid screen_sensor qnn_actor boot

install: all
	@echo "[INSTALL] Instalando binarios y servicio..."
	mkdir -p bin
	cp qernel_sim qnn_torsion pauli_daemon h7_server torsion_codec qnn_grid screen_sensor qnn_actor boot bin/
	@echo "----------------------------------------------------------"
	@echo "[OK] Binarios copiados a qernel/bin/"
	@echo "[PASO FINAL] Para instalar el servicio systemd, ejecute:"
	@echo "sudo cp qernel/qernel.service /etc/systemd/system/"
	@echo "sudo systemctl daemon-reload"
	@echo "sudo systemctl enable qernel.service"
	@echo "sudo systemctl start qernel.service"
	@echo "----------------------------------------------------------"

.PHONY: all clean install

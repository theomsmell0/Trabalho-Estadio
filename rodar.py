#!/usr/bin/env python3
import subprocess
import argparse
import os
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec

BINARIO = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                       "build/scratch/ns3.43-estadio-default")

parser = argparse.ArgumentParser(description="Simulacao do estadio")
parser.add_argument("-n", "--usuarios", type=int, default=120, help="Numero de torcedores (padrao: 120)")
parser.add_argument("-t", "--tempo",    type=int, default=30,  help="Tempo de simulacao em segundos (padrao: 30)")
parser.add_argument("-v", "--verbose",  action="store_true",   help="Logs detalhados")
parser.add_argument("--sem-grafico",    action="store_true",   help="Rodar sem abrir o grafico")
args = parser.parse_args()

cmd = [BINARIO, f"--nUsers={args.usuarios}", f"--tempo={args.tempo}"]
if args.verbose:
    cmd.append("--verbose=1")

if args.sem_grafico:
    subprocess.run(cmd)
    raise SystemExit

# --- Configuração do gráfico ---
plt.ion()
fig = plt.figure(figsize=(11, 8))
fig.suptitle(f"Trafego em tempo real  —  {args.usuarios} torcedores | 12 APs | {args.tempo}s",
             fontsize=12)

gs = gridspec.GridSpec(3, 1, hspace=0.5)
ax1 = fig.add_subplot(gs[0])
ax2 = fig.add_subplot(gs[1])
ax3 = fig.add_subplot(gs[2])

ax1.set_ylabel("Throughput (Mbps)")
ax2.set_ylabel("Taxa de entrega (%)")
ax3.set_ylabel("Atraso medio (ms)")
ax3.set_xlabel("Tempo simulado (s)")

for ax in (ax1, ax2, ax3):
    ax.set_xlim(0, args.tempo)
    ax.grid(True, linestyle="--", alpha=0.5)

ax2.set_ylim(0, 105)

line1, = ax1.plot([], [], color="steelblue",  linewidth=1.8)
line2, = ax2.plot([], [], color="seagreen",   linewidth=1.8)
line3, = ax3.plot([], [], color="firebrick",  linewidth=1.8)

times, throughputs, deliveries, delays = [], [], [], []

def atualizar_grafico():
    line1.set_data(times, throughputs)
    line2.set_data(times, deliveries)
    line3.set_data(times, delays)
    for ax, vals in [(ax1, throughputs), (ax2, deliveries), (ax3, delays)]:
        if vals:
            margin = max(vals) * 0.1 or 1
            if ax is not ax2:
                ax.set_ylim(0, max(vals) + margin)
    fig.canvas.draw()
    fig.canvas.flush_events()

def parse_stat(line):
    try:
        partes = dict(kv.split("=") for kv in line.strip().split()[1:])
        return (float(partes["t"]),
                float(partes["throughput"]),
                float(partes["entrega"]),
                float(partes["atraso"]))
    except Exception:
        return None

# --- Executar simulação e ler saída em tempo real ---
proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                        text=True, bufsize=1)

for linha in proc.stdout:
    print(linha, end="", flush=True)
    if linha.startswith("STAT "):
        stat = parse_stat(linha)
        if stat:
            t, tp, ent, atr = stat
            times.append(t)
            throughputs.append(tp)
            deliveries.append(ent)
            delays.append(atr)
            atualizar_grafico()

proc.wait()

# Manter gráfico aberto após a simulação terminar
plt.ioff()
plt.show()

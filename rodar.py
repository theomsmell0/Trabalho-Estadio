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

# --- Chart configuration ---
plt.ion()
fig = plt.figure(figsize=(16, 8))
fig.suptitle(f"Tráfego em Tempo Real\n{args.usuarios} torcedores | 6 APs | {args.tempo}s",
             fontsize=14, fontweight='bold')

gs = gridspec.GridSpec(3, 1, hspace=0.5, figure=fig)
ax1 = fig.add_subplot(gs[0])
ax2 = fig.add_subplot(gs[1])
ax3 = fig.add_subplot(gs[2])

ax1.set_ylabel("Throughput\n(Mbps)", fontsize=10, fontweight='semibold')
ax2.set_ylabel("Taxa de Entrega\n(%)", fontsize=10, fontweight='semibold')
ax3.set_ylabel("Atraso Médio\n(ms)", fontsize=10, fontweight='semibold')
ax3.set_xlabel("Tempo Simulado (s)", fontsize=10, fontweight='semibold')

for ax in (ax1, ax2, ax3):
    ax.set_xlim(0, args.tempo)
    # Smooth, modern grid and border line changes within standard backend limits
    ax.grid(True, linestyle="--", color='#ced4da', linewidth=0.6, alpha=0.7)
    ax.spines['top'].set_visible(False)
    ax.spines['right'].set_visible(False)
    ax.spines['left'].set_color('#ced4da')
    ax.spines['bottom'].set_color('#ced4da')

ax2.set_ylim(0, 105)

# Thicker and modern hex UI line colors
line1, = ax1.plot([], [], color="#1f77b4", linewidth=2.2)
line2, = ax2.plot([], [], color="#2ca02c", linewidth=2.2)
line3, = ax3.plot([], [], color="#d62728", linewidth=2.2)

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

# --- Run simulation and read output in real time ---
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

# Keep chart open after simulation ends
plt.ioff()
plt.show()

#!/usr/bin/env python3
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from matplotlib.patches import FancyArrowPatch

fig, ax = plt.subplots(figsize=(14, 10))
ax.set_xlim(0, 14)
ax.set_ylim(0, 10)
ax.axis("off")
fig.patch.set_facecolor("#f8f9fa")
ax.set_facecolor("#f8f9fa")

# ── helpers ──────────────────────────────────────────────────────────────────
def box(x, y, label, color, fontsize=9, width=1.6, height=0.55):
    rect = mpatches.FancyBboxPatch(
        (x - width / 2, y - height / 2), width, height,
        boxstyle="round,pad=0.08", linewidth=1.4,
        edgecolor="#333333", facecolor=color, zorder=3
    )
    ax.add_patch(rect)
    ax.text(x, y, label, ha="center", va="center",
            fontsize=fontsize, fontweight="bold", zorder=4)

def line(x1, y1, x2, y2, label="", color="#555555", lw=2, style="-"):
    ax.plot([x1, x2], [y1, y2], color=color, linewidth=lw,
            linestyle=style, zorder=2)
    if label:
        mx, my = (x1 + x2) / 2, (y1 + y2) / 2
        ax.text(mx + 0.12, my, label, fontsize=7.5, color="#c0392b",
                va="center", fontweight="bold", zorder=5)

def wifi_arc(x, y):
    for r, alpha in [(0.28, 0.9), (0.48, 0.6), (0.68, 0.35)]:
        arc = mpatches.Arc((x, y), r * 2, r * 2,
                           angle=0, theta1=200, theta2=340,
                           color="#2980b9", linewidth=1.4, alpha=alpha, zorder=3)
        ax.add_patch(arc)

# ── nós principais ───────────────────────────────────────────────────────────
CX = 7.0   # centro horizontal

# Servidor
SY = 9.0
box(CX, SY, "Servidor", "#d5e8d4")

# Core
CY = 7.6
box(CX, CY, "Switch Core", "#dae8fc")
line(CX, SY - 0.28, CX, CY + 0.28, "10 Gbps / 1 ms", lw=2.8, color="#27ae60")

# Distribuição
DA_X, DB_X = 4.0, 10.0
DY = 6.1
box(DA_X, DY, "Dist-A\n(Lado A)", "#fff2cc", fontsize=8)
box(DB_X, DY, "Dist-B\n(Lado B)", "#fff2cc", fontsize=8)
line(CX - 0.8, CY - 0.28, DA_X + 0.8, DY + 0.28, "1 Gbps / 2 ms", lw=2.2, color="#2980b9")
line(CX + 0.8, CY - 0.28, DB_X - 0.8, DY + 0.28, "1 Gbps / 2 ms", lw=2.2, color="#2980b9")

# ── APs Lado A ───────────────────────────────────────────────────────────────
AY = 4.3
ap_a_x = [2.0, 4.0, 6.0]
ap_a_labels = ["AP-A1\n(110, +40)", "AP-A2\n(120, 0)", "AP-A3\n(110, -40)"]

for i, (ax_x, lbl) in enumerate(zip(ap_a_x, ap_a_labels)):
    box(ax_x, AY, lbl, "#f8cecc", fontsize=7.5)
    line(DA_X, DY - 0.28, ax_x, AY + 0.28, "1 Gbps\n/ 1 ms", lw=1.8, color="#8e44ad")

# ── APs Lado B ───────────────────────────────────────────────────────────────
ap_b_x = [8.0, 10.0, 12.0]
ap_b_labels = ["AP-B1\n(-110,+40)", "AP-B2\n(-120, 0)", "AP-B3\n(-110,-40)"]

for i, (bx_x, lbl) in enumerate(zip(ap_b_x, ap_b_labels)):
    box(bx_x, AY, lbl, "#f8cecc", fontsize=7.5)
    line(DB_X, DY - 0.28, bx_x, AY + 0.28, "1 Gbps\n/ 1 ms", lw=1.8, color="#8e44ad")

# ── Wi-Fi + STAs ─────────────────────────────────────────────────────────────
STA_Y = 2.2
all_ap_x = ap_a_x + ap_b_x

for ap_x in all_ap_x:
    # sinal Wi-Fi
    wifi_arc(ap_x, AY - 0.28)
    # linha tracejada Wi-Fi
    line(ap_x, AY - 0.55, ap_x, STA_Y + 0.28,
         style="--", lw=1.4, color="#2980b9")

# Barra de STAs (torcedores)
rect = mpatches.FancyBboxPatch(
    (0.6, STA_Y - 0.28), 12.8, 0.56,
    boxstyle="round,pad=0.1", linewidth=1.2,
    edgecolor="#333", facecolor="#e1d5e7", zorder=3
)
ax.add_patch(rect)
ax.text(7.0, STA_Y, "Torcedores (STAs)  —  até 120 dispositivos Wi-Fi",
        ha="center", va="center", fontsize=9, fontweight="bold", zorder=4)

# ── legenda de tecnologias ────────────────────────────────────────────────────
legend_items = [
    mpatches.Patch(color="#27ae60", label="Fibra 10 Gbps (backbone)"),
    mpatches.Patch(color="#2980b9", label="Fibra 1 Gbps (distribuição / AP)"),
    mpatches.Patch(color="#8e44ad", label="Fibra 1 Gbps (uplink AP)"),
    mpatches.Patch(color="#2980b9", alpha=0.5, label="Wi-Fi 802.11ac (rádio)"),
]
ax.legend(handles=legend_items, loc="lower left",
          fontsize=8, framealpha=0.9, edgecolor="#aaa")

# ── anotação Wi-Fi ────────────────────────────────────────────────────────────
ax.text(7.0, 3.1, "Wi-Fi 802.11ac  |  500 Kbps / torcedor  |  raio 50 m por AP",
        ha="center", va="center", fontsize=8, color="#2980b9",
        style="italic")

ax.set_title("Topologia da Rede — Simulação Multi-AP Estádio de Futebol",
             fontsize=13, fontweight="bold", pad=12)

plt.tight_layout()
plt.savefig("topologia.png", dpi=150, bbox_inches="tight")
print("Imagem salva em: topologia.png")
plt.show()

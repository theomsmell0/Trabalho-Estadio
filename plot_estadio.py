#!/usr/bin/env python3
"""
Gera um grafico mostrando a posicao de cada torcedor (usuario) e de cada
torre Wi-Fi da simulacao 'estadio.cc'.

Le os arquivos gerados pela simulacao:
  - estadio-posicoes.csv : id, setor, torre, x, y   (usuarios)
  - estadio-torres.csv   : torre, setor, x, y, z     (torres / APs)

Uso:
  python3 plot_estadio.py
  python3 plot_estadio.py --raio 50 --out estadio.png
"""

import argparse
import csv
import math

import matplotlib.pyplot as plt
from matplotlib.patches import Ellipse, Circle, Rectangle


def ler_csv(caminho):
    with open(caminho, newline="") as f:
        return list(csv.DictReader(f))


def main():
    ap = argparse.ArgumentParser(description="Plota usuarios e torres do estadio")
    ap.add_argument("--posicoes", default="estadio-posicoes.csv")
    ap.add_argument("--torres", default="estadio-torres.csv")
    ap.add_argument("--raio", type=float, default=50.0,
                    help="Raio de alcance de cada torre (m)")
    ap.add_argument("--out", default="estadio-mapa.png")
    args = ap.parse_args()

    usuarios = ler_csv(args.posicoes)
    torres = ler_csv(args.torres)

    # Uma cor por torre
    n_torres = len(torres)
    cmap = plt.get_cmap("tab10" if n_torres <= 10 else "tab20")
    cor_torre = {t["torre"]: cmap(i % cmap.N) for i, t in enumerate(torres)}

    fig, ax = plt.subplots(figsize=(12, 7))

    # --- Contorno do oval (arquibancada) e do campo, para dar contexto ---
    # Semi-eixos usados na simulacao (raioA, raioB e o retangulo do campo).
    ax.add_patch(Ellipse((0, 0), 2 * 130, 2 * 62, fill=False,
                         edgecolor="#888", linestyle="--", linewidth=1.2))
    ax.add_patch(Rectangle((-55, -35), 110, 70, fill=True,
                           facecolor="#dff0df", edgecolor="#3a7d3a",
                           linewidth=1.5, zorder=0))
    ax.plot([0, 0], [-35, 35], color="#3a7d3a", linewidth=1)  # linha do meio

    # --- Usuarios (torcedores), coloridos pela torre a que pertencem ---
    for t in torres:
        tid = t["torre"]
        xs = [float(u["x"]) for u in usuarios if u["torre"] == tid]
        ys = [float(u["y"]) for u in usuarios if u["torre"] == tid]
        ax.scatter(xs, ys, s=14, color=cor_torre[tid], alpha=0.75,
                   edgecolors="none", label=f"Usuarios TORRE-{tid} ({len(xs)})",
                   zorder=2)

    # --- Torres e seu raio de cobertura ---
    for t in torres:
        tid = t["torre"]
        x, y = float(t["x"]), float(t["y"])
        c = cor_torre[tid]
        ax.add_patch(Circle((x, y), args.raio, fill=True, facecolor=c,
                            alpha=0.08, edgecolor=c, linestyle=":", zorder=1))
        ax.scatter([x], [y], marker="^", s=260, color=c,
                   edgecolors="black", linewidths=1.4, zorder=4)
        ax.annotate(f"T{tid}", (x, y), color="black", fontsize=9,
                    fontweight="bold", ha="center", va="center", zorder=5)

    ax.set_aspect("equal")
    ax.set_xlabel("X (m)")
    ax.set_ylabel("Y (m)")
    ax.set_title(f"Estadio: {len(usuarios)} usuarios e {n_torres} torres Wi-Fi")
    ax.grid(True, linestyle=":", alpha=0.4)
    ax.legend(loc="center left", bbox_to_anchor=(1.01, 0.5), fontsize=8,
              framealpha=0.9)

    fig.tight_layout()
    fig.savefig(args.out, dpi=150, bbox_inches="tight")
    print(f"Grafico salvo em: {args.out}")
    print(f"  {len(usuarios)} usuarios, {n_torres} torres")


if __name__ == "__main__":
    main()

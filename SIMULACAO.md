# Simulação Multi-AP — Estádio de Futebol

Simulação de rede Wi-Fi 802.11ac em um estádio de futebol usando **ns-3.43**.  
Modela o comportamento de dezenas a centenas de torcedores conectados simultaneamente a 6 torres de acesso.

---

## Objetivo

Avaliar o desempenho de uma rede Wi-Fi densa sob carga elevada, medindo:

- **Throughput total** da rede (Mbps)
- **Atraso médio** de entrega de pacotes (ms)
- **Taxa de entrega** de pacotes (%)
- **Taxa de perda** de pacotes (%)

Os resultados permitem dimensionar a infraestrutura de rede para eventos de grande porte.

---

## Topologia

```
        [Servidor]
            | 10 Gbps / 1 ms
         [Core]
        /         \  1 Gbps / 2 ms
   [Dist-A]     [Dist-B]
   /  |  \       /  |  \
  A1  A2  A3   B1  B2  B3   ← 6 torres Wi-Fi 802.11ac
```

- **1 Servidor** de conteúdo central (downlink para os torcedores)
- **1 Switch core** com links de 10 Gbps
- **2 Switches de distribuição** (um por lado do estádio), 1 Gbps
- **6 Access Points** (3 no Lado A, 3 no Lado B), 1 Gbps por uplink
- **N torcedores** (padrão: 120) distribuídos aleatoriamente entre as torres

### Layout físico (metros)

```
(-110, +40)   [AP-B1]          [AP-A1]   (+110, +40)
(-120,   0)   [AP-B2] [CAMPO]  [AP-A2]   (+120,   0)
(-110, -40)   [AP-B3]          [AP-A3]   (+110, -40)
   Lado B                                    Lado A
```

Raio de cobertura de cada torre: **50 m**. Usuários em zonas de sobreposição são sorteados aleatoriamente entre as torres adjacentes.

---

## Tráfego simulado

- **Padrão:** UDP OnOff (streaming / redes sociais)
- **Taxa por torcedor:** 500 Kbps downlink (servidor → torcedor)
- **Tamanho de pacote:** 1024 bytes
- **Padrão On/Off:** tempo On constante (1 s), tempo Off exponencial (média 0,5 s)

---

## Pré-requisitos

- ns-3.43 compilado com suporte a Wi-Fi e FlowMonitor
- Python 3 com `matplotlib` (para o script com gráfico em tempo real)

### Compilar o ns-3 (primeira vez)

```bash
cd ns-allinone-3.43/ns-3.43
./ns3 configure --enable-examples
./ns3 build
```

---

## Como executar

### Opção 1 — Via ns3 (direto)

```bash
cd ns-allinone-3.43/ns-3.43

# Simulação padrão (120 torcedores, 30 segundos)
./ns3 run "scratch/estadio-stress"

# Personalizar número de usuários e duração
./ns3 run "scratch/estadio-stress --nUsers=60 --tempo=20"

# Com logs detalhados
./ns3 run "scratch/estadio-stress --nUsers=120 --verbose=1"
```

### Opção 2 — Via script Python (com gráfico em tempo real)

```bash
cd ns-allinone-3.43/ns-3.43
python3 rodar.py

# Opções disponíveis:
python3 rodar.py --usuarios 60 --tempo 20
python3 rodar.py --usuarios 120 --verbose
python3 rodar.py --sem-grafico    # sem janela gráfica
```

> O script `rodar.py` chama o binário compilado diretamente e plota throughput,
> taxa de entrega e atraso em tempo real enquanto a simulação roda.

---

## Parâmetros

| Parâmetro    | Padrão | Descrição                         |
|--------------|--------|-----------------------------------|
| `--nUsers`   | `120`  | Total de torcedores no estádio    |
| `--tempo`    | `30`   | Duração da simulação (segundos)   |
| `--verbose`  | `false`| Ativar logs detalhados do ns-3    |

---

## Arquivos de saída

| Arquivo                         | Conteúdo                                              |
|---------------------------------|-------------------------------------------------------|
| `estadio-metricas.csv`          | Resumo das métricas por execução (append)             |
| `estadio-posicoes.csv`          | Posição (x, y) de cada torcedor por torre/lado        |
| `estadio-flowmon-<N>.xml`       | Dados brutos do FlowMonitor (N = número de usuários)  |

### Exemplo de `estadio-metricas.csv`

```
usuarios,nos_total,fluxos,throughput_mbps,atraso_ms,...
120,138,116,40.63,5.35,136275,131514,3123,96.51,3.49
```

---

## Saída em tempo real (durante a simulação)

A cada segundo simulado, o programa imprime uma linha `STAT`:

```
STAT t=1.0 throughput=15.32 atraso=4.21 entrega=98.50 tx=4200 rx=4137 lost=63
```

O script `rodar.py` lê essas linhas e atualiza o gráfico em tempo real.

---

## Resultados esperados

Com 120 torcedores e 6 APs:

- **Throughput total:** ~40 Mbps
- **Atraso médio:** ~5 ms
- **Taxa de entrega:** ~96–97%
- **Taxa de perda:** ~3–4%

Para poucos usuários (ex.: 10), o throughput sobe significativamente (~230 Mbps) e a perda cai para menos de 1%.

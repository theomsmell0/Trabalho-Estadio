/*
 * Simulação Multi-AP - Estádio de Futebol
 * 2 lados × 3 torres Wi-Fi 802.11ac = 6 APs
 * Apenas torcedores (120 usuários padrão)
 * Sem handover — atribuição aleatória em zonas de sobreposição
 *
 * Uso: ./ns3 run "scratch/estadio-stress --nUsers=120"
 */

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"

#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("EstadioMultiAP");

struct Metricas
{
    double throughputTotal = 0;
    double atrasoMedio     = 0;
    double taxaEntrega     = 0;
    uint64_t pacEnviados   = 0;
    uint64_t pacRecebidos  = 0;
    uint64_t pacPerdidos   = 0;
};

// Amostra o FlowMonitor a cada segundo simulado e imprime uma linha STAT.
// O Python lê essas linhas para plotar em tempo real.
void
AmostraMetricas(Ptr<FlowMonitor> monitor, double intervalo, double tempoFim)
{
    monitor->CheckForLostPackets();
    auto stats = monitor->GetFlowStats();

    double throughput = 0;
    uint64_t totalTx = 0, totalRx = 0, totalLost = 0;
    double somaAtraso = 0;
    uint32_t fluxosComAtraso = 0;

    for (auto it = stats.begin(); it != stats.end(); ++it)
    {
        auto& fs = it->second;
        double dur = fs.timeLastRxPacket.GetSeconds() - fs.timeFirstTxPacket.GetSeconds();
        if (dur > 0 && fs.rxPackets > 0)
            throughput += (fs.rxBytes * 8.0) / dur / 1e6;
        if (fs.rxPackets > 0)
        {
            somaAtraso += fs.delaySum.GetSeconds() / fs.rxPackets * 1000.0;
            fluxosComAtraso++;
        }
        totalTx   += fs.txPackets;
        totalRx   += fs.rxPackets;
        totalLost += fs.lostPackets;
    }

    double atraso  = fluxosComAtraso > 0 ? somaAtraso / fluxosComAtraso : 0;
    double entrega = totalTx > 0 ? (double)totalRx / totalTx * 100.0 : 0;

    std::cout << "STAT"
              << " t="          << std::fixed << std::setprecision(1)
                                << Simulator::Now().GetSeconds()
              << " throughput=" << std::setprecision(2) << throughput
              << " atraso="     << atraso
              << " entrega="    << entrega
              << " tx="         << totalTx
              << " rx="         << totalRx
              << " lost="       << totalLost
              << std::endl;

    if (Simulator::Now().GetSeconds() + intervalo < tempoFim)
        Simulator::Schedule(Seconds(intervalo), &AmostraMetricas, monitor, intervalo, tempoFim);
}

// Instala Wi-Fi 802.11ac para um AP e seus STAs.
// Retorna InterfaceContainer: índice 0 = AP, 1..N = STAs.
Ipv4InterfaceContainer
InstalarAP(Ptr<Node> apNode,
           NodeContainer& staNodes,
           const std::string& ssid,
           double apX,
           double apY,
           double raio,
           Ipv4AddressHelper& ipHelper)
{
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ac);
    wifi.SetRemoteStationManager("ns3::MinstrelHtWifiManager");

    WifiMacHelper mac;
    Ssid wssid = Ssid(ssid);

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(wssid));
    NetDeviceContainer apDev = wifi.Install(phy, mac, apNode);

    mac.SetType("ns3::StaWifiMac",
                "Ssid",          SsidValue(wssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDev = wifi.Install(phy, mac, staNodes);

    MobilityHelper mob;

    Ptr<ListPositionAllocator> apPos = CreateObject<ListPositionAllocator>();
    apPos->Add(Vector(apX, apY, 15.0));
    mob.SetPositionAllocator(apPos);
    mob.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mob.Install(apNode);

    std::ostringstream rhoStr;
    rhoStr << "ns3::UniformRandomVariable[Min=1.0|Max=" << raio << "]";
    mob.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                             "X",   DoubleValue(apX),
                             "Y",   DoubleValue(apY),
                             "Rho", StringValue(rhoStr.str()));
    mob.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mob.Install(staNodes);

    NetDeviceContainer all;
    all.Add(apDev);
    all.Add(staDev);
    return ipHelper.Assign(all);
}

int
main(int argc, char* argv[])
{
    uint32_t nUsers  = 120;
    double   simTime = 30.0;
    bool     verbose = false;

    CommandLine cmd;
    cmd.AddValue("nUsers",  "Total de torcedores no estádio", nUsers);
    cmd.AddValue("tempo",   "Tempo de simulação em segundos", simTime);
    cmd.AddValue("verbose", "Ativar logs detalhados", verbose);
    cmd.Parse(argc, argv);

    if (verbose)
        LogComponentEnable("EstadioMultiAP", LOG_LEVEL_INFO);

    // Metade por lado (Lado A absorve ímpares)
    uint32_t nLadoB = nUsers / 2;
    uint32_t nLadoA = nUsers - nLadoB;

    std::cout << "\n============================================\n"
              << "  SIMULAÇÃO MULTI-AP - ESTÁDIO DE FUTEBOL\n"
              << "    Wi-Fi 802.11ac | 2 lados × 3 torres\n"
              << "============================================\n"
              << "Total de torcedores : " << nUsers << "\n"
              << "  Lado A : " << nLadoA << " | Lado B : " << nLadoB << "\n"
              << "Torres Wi-Fi        : 6 APs (3 por lado)\n"
              << "Tempo de simulação  : " << simTime << "s\n"
              << "============================================\n\n";

    // ======================================================
    //  TOPOLOGIA
    //
    //      [Servidor]
    //          | 10Gbps
    //       [Core]
    //       /       \  1Gbps
    //   [Dist-A]  [Dist-B]
    //   /  |  \   /  |  \
    //  A1  A2  A3 B1  B2  B3   ← 6 torres Wi-Fi
    //
    //  Layout físico (metros):
    //
    //  (-110, 40)              (110, 40)
    //  (-120,  0)  [campo]    (120,  0)
    //  (-110,-40)              (110,-40)
    //   Lado B                  Lado A
    // ======================================================

    std::cout << "> [1/7] criando nos\n";

    NodeContainer serverNode; serverNode.Create(1);
    NodeContainer coreNode;   coreNode.Create(1);
    NodeContainer distNodes;  distNodes.Create(2); // 0=Lado A  1=Lado B

    NodeContainer apLadoA; apLadoA.Create(3);
    NodeContainer apLadoB; apLadoB.Create(3);

    NodeContainer ladoANodes; ladoANodes.Create(nLadoA);
    NodeContainer ladoBNodes; ladoBNodes.Create(nLadoB);

    std::cout << "> [2/7] backbone cabeado\n";

    PointToPointHelper p2p10G;
    p2p10G.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
    p2p10G.SetChannelAttribute("Delay",   StringValue("1ms"));
    NetDeviceContainer devServCore = p2p10G.Install(serverNode.Get(0), coreNode.Get(0));

    PointToPointHelper p2p1G;
    p2p1G.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    p2p1G.SetChannelAttribute("Delay",   StringValue("2ms"));
    NetDeviceContainer devCoreDistA = p2p1G.Install(coreNode.Get(0), distNodes.Get(0));
    NetDeviceContainer devCoreDistB = p2p1G.Install(coreNode.Get(0), distNodes.Get(1));

    PointToPointHelper p2pAp;
    p2pAp.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    p2pAp.SetChannelAttribute("Delay",   StringValue("1ms"));

    NetDeviceContainer devDistAp[6];
    for (int i = 0; i < 3; i++)
    {
        devDistAp[i]     = p2pAp.Install(distNodes.Get(0), apLadoA.Get(i));
        devDistAp[3 + i] = p2pAp.Install(distNodes.Get(1), apLadoB.Get(i));
    }

    std::cout << "> [3/7] pilha TCP/IP e endereçamento\n";

    InternetStackHelper internet;
    internet.Install(serverNode);
    internet.Install(coreNode);
    internet.Install(distNodes);
    internet.Install(apLadoA);
    internet.Install(apLadoB);
    internet.Install(ladoANodes);
    internet.Install(ladoBNodes);

    MobilityHelper mobFixa;
    mobFixa.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobFixa.Install(serverNode);
    mobFixa.Install(coreNode);
    mobFixa.Install(distNodes);

    Ipv4AddressHelper ipv4;

    ipv4.SetBase("10.0.0.0", "255.255.255.252");
    ipv4.Assign(devServCore);
    ipv4.SetBase("10.0.1.0", "255.255.255.252");
    ipv4.Assign(devCoreDistA);
    ipv4.SetBase("10.0.2.0", "255.255.255.252");
    ipv4.Assign(devCoreDistB);

    for (int i = 0; i < 6; i++)
    {
        std::ostringstream base;
        base << "10.1." << (i + 1) << ".0";
        ipv4.SetBase(base.str().c_str(), "255.255.255.252");
        ipv4.Assign(devDistAp[i]);
    }

    // ======================================================
    //  ATRIBUIÇÃO ALEATÓRIA ÀS TORRES
    //  Usuários em zona de sobreposição são sorteados.
    // ======================================================
    std::cout << "> [4/7] sorteando torcedores por torre\n";

    Ptr<UniformRandomVariable> rng = CreateObject<UniformRandomVariable>();
    rng->SetAttribute("Min", DoubleValue(0.0));
    rng->SetAttribute("Max", DoubleValue(3.0));

    auto sortearGrupos = [&](NodeContainer& nodes) {
        std::array<NodeContainer, 3> grupos;
        for (uint32_t i = 0; i < nodes.GetN(); i++)
        {
            int ap = std::min((int)rng->GetValue(), 2);
            grupos[ap].Add(nodes.Get(i));
        }
        return grupos;
    };

    auto gA = sortearGrupos(ladoANodes);
    auto gB = sortearGrupos(ladoBNodes);

    // ======================================================
    //  REDES WI-FI
    //  Posições baseadas na planta oval do estádio
    // ======================================================
    std::cout << "> [5/7] redes Wi-Fi por setor\n";

    struct PosAP { double x, y; };
    PosAP posA[3] = {{ 110,  40}, { 120,   0}, { 110, -40}};
    PosAP posB[3] = {{-110,  40}, {-120,   0}, {-110, -40}};

    const double raioAP = 50.0;

    Ipv4InterfaceContainer ifA[3];
    for (int i = 0; i < 3; i++)
    {
        std::ostringstream base, ssid;
        base << "192.168." << (10 + i) << ".0";
        ssid << "LADO-A-" << (i + 1);
        ipv4.SetBase(base.str().c_str(), "255.255.255.0");
        ifA[i] = InstalarAP(apLadoA.Get(i), gA[i], ssid.str(),
                            posA[i].x, posA[i].y, raioAP, ipv4);
    }

    Ipv4InterfaceContainer ifB[3];
    for (int i = 0; i < 3; i++)
    {
        std::ostringstream base, ssid;
        base << "192.168." << (20 + i) << ".0";
        ssid << "LADO-B-" << (i + 1);
        ipv4.SetBase(base.str().c_str(), "255.255.255.0");
        ifB[i] = InstalarAP(apLadoB.Get(i), gB[i], ssid.str(),
                            posB[i].x, posB[i].y, raioAP, ipv4);
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // ======================================================
    //  LOG DE POSIÇÕES DOS TORCEDORES
    //  Salvo em estadio-posicoes.csv após a alocação.
    // ======================================================
    {
        std::ofstream posCSV("estadio-posicoes.csv");
        posCSV << "id,lado,torre,x,y\n";
        posCSV << std::fixed << std::setprecision(2);

        uint32_t uid = 1;
        for (int ap = 0; ap < 3; ap++)
        {
            for (uint32_t i = 0; i < gA[ap].GetN(); i++)
            {
                Vector pos = gA[ap].Get(i)->GetObject<MobilityModel>()->GetPosition();
                posCSV << uid++ << ",A," << (ap + 1) << "," << pos.x << "," << pos.y << "\n";
            }
        }
        for (int ap = 0; ap < 3; ap++)
        {
            for (uint32_t i = 0; i < gB[ap].GetN(); i++)
            {
                Vector pos = gB[ap].Get(i)->GetObject<MobilityModel>()->GetPosition();
                posCSV << uid++ << ",B," << (ap + 1) << "," << pos.x << "," << pos.y << "\n";
            }
        }
        posCSV.close();
        std::cout << "  posicoes salvas em: estadio-posicoes.csv\n";
    }

    // ======================================================
    //  TRÁFEGO: UDP OnOff — streaming / redes sociais
    //  500 Kbps por torcedor | servidor → torcedor (downlink)
    // ======================================================
    std::cout << "> [6/7] configurando trafego\n";

    uint16_t portBase = 9000;

    auto instalarFluxos = [&](std::array<NodeContainer, 3>& grupos,
                              Ipv4InterfaceContainer ifaces[3]) {
        for (int ap = 0; ap < 3; ap++)
        {
            for (uint32_t i = 0; i < grupos[ap].GetN(); i++)
            {
                PacketSinkHelper sink("ns3::UdpSocketFactory",
                                      InetSocketAddress(Ipv4Address::GetAny(), portBase));
                auto sinkApp = sink.Install(grupos[ap].Get(i));
                sinkApp.Start(Seconds(0.0));
                sinkApp.Stop(Seconds(simTime));

                OnOffHelper onoff("ns3::UdpSocketFactory",
                                  InetSocketAddress(ifaces[ap].GetAddress(i + 1), portBase));
                onoff.SetAttribute("DataRate",   StringValue("500Kbps"));
                onoff.SetAttribute("PacketSize", UintegerValue(1024));
                onoff.SetAttribute("OnTime",
                    StringValue("ns3::ConstantRandomVariable[Constant=1]"));
                onoff.SetAttribute("OffTime",
                    StringValue("ns3::ExponentialRandomVariable[Mean=0.5]"));

                double t = 1.0 + (portBase - 9000) * 0.01;
                if (t > simTime - 2.0)
                    t = 1.0 + fmod((portBase - 9000) * 0.01, simTime - 3.0);
                auto onoffApp = onoff.Install(serverNode.Get(0));
                onoffApp.Start(Seconds(t));
                onoffApp.Stop(Seconds(simTime - 0.5));

                portBase++;
            }
        }
    };

    instalarFluxos(gA, ifA);
    instalarFluxos(gB, ifB);

    // ======================================================
    //  MONITORAMENTO
    // ======================================================
    std::cout << "> [7/7] monitoramento de fluxo\n\n";

    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> flowMonitor = flowHelper.InstallAll();

    Simulator::Schedule(Seconds(1.0), &AmostraMetricas, flowMonitor, 1.0, simTime);

    Simulator::Stop(Seconds(simTime));
    std::cout << "--------------------------------------------\n"
              << " simulando " << simTime << "s com " << nUsers << " torcedores\n"
              << "--------------------------------------------\n";
    Simulator::Run();
    std::cout << " concluido.\n\n";

    // ======================================================
    //  COLETA FINAL DE MÉTRICAS
    // ======================================================
    flowMonitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier =
        DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();

    Metricas m;
    uint32_t fluxosAtivos    = 0;
    double   somaAtrasos     = 0;
    uint32_t fluxosComAtraso = 0;

    for (auto it = stats.begin(); it != stats.end(); ++it)
    {
        FlowMonitor::FlowStats fs = it->second;
        if (fs.rxPackets > 0)
        {
            fluxosAtivos++;
            double dur = fs.timeLastRxPacket.GetSeconds() -
                         fs.timeFirstTxPacket.GetSeconds();
            if (dur > 0)
                m.throughputTotal += (fs.rxBytes * 8.0) / dur / 1e6;
            somaAtrasos += fs.delaySum.GetSeconds() / fs.rxPackets * 1000.0;
            fluxosComAtraso++;
        }
        m.pacEnviados  += fs.txPackets;
        m.pacRecebidos += fs.rxPackets;
        m.pacPerdidos  += fs.lostPackets;
    }

    if (fluxosComAtraso > 0)
        m.atrasoMedio = somaAtrasos / fluxosComAtraso;
    if (m.pacEnviados > 0)
        m.taxaEntrega = (double)m.pacRecebidos / m.pacEnviados * 100.0;

    uint32_t nosTotal = 1 + 1 + 2 + 6 + nUsers; // server+core+dist+APs+torcedores

    std::cout << "============================================\n"
              << "  RESULTADOS — " << nUsers << " TORCEDORES | 6 APs\n"
              << "============================================\n"
              << std::fixed << std::setprecision(2)
              << "Fluxos ativos    : " << fluxosAtivos       << "\n"
              << "Throughput total : " << m.throughputTotal  << " Mbps\n"
              << "Atraso medio     : " << m.atrasoMedio      << " ms\n"
              << "Pacotes enviados : " << m.pacEnviados      << "\n"
              << "Pacotes recebidos: " << m.pacRecebidos     << "\n"
              << "Pacotes perdidos : " << m.pacPerdidos      << "\n"
              << "Taxa de entrega  : " << m.taxaEntrega      << "%\n"
              << "Taxa de perda    : " << 100.0 - m.taxaEntrega << "%\n"
              << "============================================\n\n";

    // CSV de métricas
    std::string csvFile = "estadio-metricas.csv";
    bool fileExists = false;
    { std::ifstream chk(csvFile); fileExists = chk.good(); }

    std::ofstream csv(csvFile, std::ios::app);
    if (!fileExists)
        csv << "usuarios,nos_total,fluxos,throughput_mbps,atraso_ms,"
               "pkts_enviados,pkts_recebidos,pkts_perdidos,"
               "taxa_entrega_pct,taxa_perda_pct\n";

    csv << nUsers << "," << nosTotal << "," << fluxosAtivos << ","
        << std::fixed << std::setprecision(2)
        << m.throughputTotal << "," << m.atrasoMedio << ","
        << m.pacEnviados << "," << m.pacRecebidos << ","
        << m.pacPerdidos << "," << m.taxaEntrega << ","
        << 100.0 - m.taxaEntrega << "\n";
    csv.close();

    std::cout << "Metricas salvas em : estadio-metricas.csv\n";

    std::string xmlFile = "estadio-flowmon-" + std::to_string(nUsers) + ".xml";
    flowMonitor->SerializeToXmlFile(xmlFile, true, true);
    std::cout << "FlowMonitor XML    : " << xmlFile << "\n";
    std::cout << "Posicoes           : estadio-posicoes.csv\n\n";

    Simulator::Destroy();
    return 0;
}

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
#include <vector>

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
           double campoX,
           double campoY,
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

    // Sorteia posições dentro do disco da torre, rejeitando pontos que caiam
    // dentro do retângulo do campo de futebol (evita torcedores em campo).
    Ptr<UniformRandomVariable> rngAngulo = CreateObject<UniformRandomVariable>();
    rngAngulo->SetAttribute("Min", DoubleValue(0.0));
    rngAngulo->SetAttribute("Max", DoubleValue(2.0 * M_PI));

    Ptr<UniformRandomVariable> rngRaio = CreateObject<UniformRandomVariable>();
    rngRaio->SetAttribute("Min", DoubleValue(0.0));
    rngRaio->SetAttribute("Max", DoubleValue(1.0));

    Ptr<ListPositionAllocator> staPos = CreateObject<ListPositionAllocator>();
    for (uint32_t i = 0; i < staNodes.GetN(); i++)
    {
        double x, y;
        do
        {
            double theta = rngAngulo->GetValue();
            double r     = raio * std::sqrt(rngRaio->GetValue());
            x = apX + r * std::cos(theta);
            y = apY + r * std::sin(theta);
        } while (std::abs(x) < campoX && std::abs(y) < campoY);

        staPos->Add(Vector(x, y, 0.0));
    }
    mob.SetPositionAllocator(staPos);
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
    uint32_t nTorres = 6;    // torres Wi-Fi distribuídas ao redor de todo o oval
    double   simTime = 30.0;
    bool     verbose = false;

    CommandLine cmd;
    cmd.AddValue("nUsers",  "Total de torcedores no estádio", nUsers);
    cmd.AddValue("nTorres", "Numero de torres Wi-Fi ao redor do oval", nTorres);
    cmd.AddValue("tempo",   "Tempo de simulação em segundos", simTime);
    cmd.AddValue("verbose", "Ativar logs detalhados", verbose);
    cmd.Parse(argc, argv);

    if (verbose)
        LogComponentEnable("EstadioMultiAP", LOG_LEVEL_INFO);

    if (nTorres < 4)
        nTorres = 4; // mínimo para manter 4 setores de backbone

    // Backbone dividido em 4 setores (quadrantes) do oval.
    // Cada setor concentra um trecho de torres vizinhas.
    const uint32_t nSetores = 4;

    std::cout << "\n============================================\n"
              << "  SIMULAÇÃO MULTI-AP - ESTÁDIO DE FUTEBOL\n"
              << "    Wi-Fi 802.11ac | torres ao redor do oval\n"
              << "============================================\n"
              << "Total de torcedores : " << nUsers  << "\n"
              << "Torres Wi-Fi        : " << nTorres << " (" << nSetores << " setores)\n"
              << "Tempo de simulação  : " << simTime << "s\n"
              << "============================================\n\n";

    std::cout << "> [1/7] criando nos\n";

    NodeContainer serverNode; serverNode.Create(1);
    NodeContainer coreNode;   coreNode.Create(1);
    NodeContainer distNodes;  distNodes.Create(nSetores);

    NodeContainer apNodes;    apNodes.Create(nTorres);
    NodeContainer torcedores; torcedores.Create(nUsers);

    std::cout << "> [2/7] backbone cabeado\n";

    PointToPointHelper p2p10G;
    p2p10G.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
    p2p10G.SetChannelAttribute("Delay",   StringValue("1ms"));
    NetDeviceContainer devServCore = p2p10G.Install(serverNode.Get(0), coreNode.Get(0));

    PointToPointHelper p2p1G;
    p2p1G.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    p2p1G.SetChannelAttribute("Delay",   StringValue("2ms"));

    std::vector<NetDeviceContainer> devCoreDist(nSetores);
    for (uint32_t s = 0; s < nSetores; s++)
        devCoreDist[s] = p2p1G.Install(coreNode.Get(0), distNodes.Get(s));

    PointToPointHelper p2pAp;
    p2pAp.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    p2pAp.SetChannelAttribute("Delay",   StringValue("1ms"));

    // Cada torre pertence ao setor cujo arco ela ocupa no oval.
    std::vector<uint32_t> setorDaTorre(nTorres);
    std::vector<NetDeviceContainer> devDistAp(nTorres);
    for (uint32_t i = 0; i < nTorres; i++)
    {
        uint32_t s = (i * nSetores) / nTorres;
        setorDaTorre[i] = s;
        devDistAp[i] = p2pAp.Install(distNodes.Get(s), apNodes.Get(i));
    }

    std::cout << "> [3/7] pilha TCP/IP e endereçamento\n";

    InternetStackHelper internet;
    internet.Install(serverNode);
    internet.Install(coreNode);
    internet.Install(distNodes);
    internet.Install(apNodes);
    internet.Install(torcedores);

    MobilityHelper mobFixa;
    mobFixa.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobFixa.Install(serverNode);
    mobFixa.Install(coreNode);
    mobFixa.Install(distNodes);

    Ipv4AddressHelper ipv4;

    ipv4.SetBase("10.0.0.0", "255.255.255.252");
    ipv4.Assign(devServCore);

    for (uint32_t s = 0; s < nSetores; s++)
    {
        std::ostringstream base;
        base << "10.0." << (s + 1) << ".0";
        ipv4.SetBase(base.str().c_str(), "255.255.255.252");
        ipv4.Assign(devCoreDist[s]);
    }

    for (uint32_t i = 0; i < nTorres; i++)
    {
        std::ostringstream base;
        base << "10.1." << (i + 1) << ".0";
        ipv4.SetBase(base.str().c_str(), "255.255.255.252");
        ipv4.Assign(devDistAp[i]);
    }

    std::cout << "> [4/7] posicionando torres ao redor do oval\n";

    const double raioA = 120.0; // semi-eixo maior (laterais do campo)
    const double raioB = 55.0;  // semi-eixo menor (atrás dos gols)
    const double raioAP = 50.0; // raio de alcance de cada torre

    // Retângulo do campo de futebol (área de jogo), onde torcedores NÃO podem
    // ser gerados. Medidas em metros a partir do centro (0,0).
    const double campoX = 55.0; // metade do comprimento do campo
    const double campoY = 35.0; // metade da largura do campo

    struct PosAP { double x, y; };
    std::vector<PosAP> posAPs(nTorres);
    for (uint32_t i = 0; i < nTorres; i++)
    {
        double theta = 2.0 * M_PI * i / nTorres;
        posAPs[i] = { raioA * std::cos(theta), raioB * std::sin(theta) };
    }

    std::cout << "> [5/7] sorteando torcedores por torre\n";

    Ptr<UniformRandomVariable> rng = CreateObject<UniformRandomVariable>();
    rng->SetAttribute("Min", DoubleValue(0.0));
    rng->SetAttribute("Max", DoubleValue((double)nTorres));

    std::vector<NodeContainer> grupos(nTorres);
    for (uint32_t i = 0; i < torcedores.GetN(); i++)
    {
        uint32_t ap = std::min((uint32_t)rng->GetValue(), nTorres - 1);
        grupos[ap].Add(torcedores.Get(i));
    }


    std::cout << "> [6/9] redes Wi-Fi por setor\n";

    std::vector<Ipv4InterfaceContainer> ifaces(nTorres);
    for (uint32_t i = 0; i < nTorres; i++)
    {
        std::ostringstream base, ssid;
        base << "192.168." << (10 + i) << ".0";
        ssid << "TORRE-" << (i + 1);
        ipv4.SetBase(base.str().c_str(), "255.255.255.0");
        ifaces[i] = InstalarAP(apNodes.Get(i), grupos[i], ssid.str(),
                               posAPs[i].x, posAPs[i].y, raioAP,
                               campoX, campoY, ipv4);
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

   
    {
        std::ofstream posCSV("estadio-posicoes.csv");
        posCSV << "id,setor,torre,x,y\n";
        posCSV << std::fixed << std::setprecision(2);

        uint32_t uid = 1;
        for (uint32_t ap = 0; ap < nTorres; ap++)
        {
            for (uint32_t i = 0; i < grupos[ap].GetN(); i++)
            {
                Vector pos = grupos[ap].Get(i)->GetObject<MobilityModel>()->GetPosition();
                posCSV << uid++ << "," << setorDaTorre[ap] << ","
                       << (ap + 1) << "," << pos.x << "," << pos.y << "\n";
            }
        }
        posCSV.close();
        std::cout << "  posicoes salvas em: estadio-posicoes.csv\n";
    }

// NOVO: Salvando as posições originais das Torres (APs)
    {
        std::ofstream apCSV("estadio-torres.csv");
        apCSV << "torre,setor,x,y,z\n";
        apCSV << std::fixed << std::setprecision(2);

        for (uint32_t ap = 0; ap < nTorres; ap++)
        {
            // Pega a coordenada XYZ real direto do módulo de mobilidade do NS-3
            Vector pos = apNodes.Get(ap)->GetObject<MobilityModel>()->GetPosition();
            
            apCSV << (ap + 1) << "," << setorDaTorre[ap] << ","
                  << pos.x << "," << pos.y << "," << pos.z << "\n";
        }
        apCSV.close();
        std::cout << "  posicoes das torres salvas em: estadio-torres.csv\n";
    }
   
    std::cout << "> [7/9] configurando trafego\n";

    uint16_t portBase = 9000;

    for (uint32_t ap = 0; ap < nTorres; ap++)
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


    std::cout << "> [8/9] monitoramento de fluxo\n\n";

    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> flowMonitor = flowHelper.InstallAll();

    Simulator::Schedule(Seconds(1.0), &AmostraMetricas, flowMonitor, 1.0, simTime);

    Simulator::Stop(Seconds(simTime));
    std::cout << "--------------------------------------------\n"
              << " simulando " << simTime << "s com " << nUsers << " torcedores\n"
              << "--------------------------------------------\n";
    Simulator::Run();
    std::cout << " concluido.\n\n";

  
    std::cout << "> [9/9] coletando metricas\n";

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

    uint32_t nosTotal = 1 + 1 + nSetores + nTorres + nUsers; // server+core+dist+APs+torcedores

    std::cout << "============================================\n"
              << "  RESULTADOS — " << nUsers << " TORCEDORES | " << nTorres << " APs\n"
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

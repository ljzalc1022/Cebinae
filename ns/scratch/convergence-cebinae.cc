#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-id-tag.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"

#include <iostream>
#include <iomanip>

using namespace ns3;

/* 
            S1(5)                      R1(5)
     25Gbps |          25Gbp           | 25Gbps
            T1 ----------------------- T2
    4 flows start at 0.000s, 0.001s, 0.002s, 0.003s
    5th flow starts at 0.100s
*/
uint32_t N = 5; // number of flows
Time extraFlowStartTime = Seconds(0.100);

Time startTime = Seconds(0);
Time stopTime = Seconds(0.15);
const std::string FlowDataRate = "25Gbps";
const std::string bottleneckDataRate = "25Gbps";
const uint64_t bottleneckDataRate_n = 25e9;

const Time progressInterval = Seconds(0.01);
void
PrintProgress()
{
    std::cout << "Progress to " 
              << std::fixed << std::setprecision(2) << Simulator::Now().GetSeconds() 
              << " seconds simulation time"  << std::endl;
    Simulator::Schedule(progressInterval, &PrintProgress);
}

std::vector<uint64_t> rxS1R1Bytes;
void
InitializeCounters()
{
    for (uint32_t i = 0; i < N; i++)
    {
        rxS1R1Bytes[i] = 0;
    }
}
void
TraceSink(std::size_t index, Ptr<const Packet> p, const Address& a)
{
    rxS1R1Bytes[index] += p->GetSize();
}

// use flow id tag to infer the sender
void 
TraceSender(uint32_t nodeId, Ptr<const Packet> packet)
{
    packet->AddByteTag(FlowIdTag(nodeId));
}

// output files
// 1. caseName-S1R1-throughput.dat 
//      -- small flow throughput v.s. time
// 2. caseName-bottlenet-utilization.dat 
//      -- bottleneck bandwidth utilization v.s. time
// 3. caseName-qlen.dat
//      -- qlen of bottleneck v.s. time
// 4. caseName-Jain.dat
//      -- Jain's fairness index: https://en.wikipedia.org/wiki/Fairness_measure
std::ofstream rxS1R1Throughput;
std::ofstream bottlenetUtilization;
std::ofstream qlen_f;
std::ofstream Jain_f;
const Time measurementInterval = MilliSeconds(1);
const double measurementInterval_d = 0.001;
void PrintThroughput()
{
    static bool flag95 = false, flag99 = false, flag999 = false;
    uint64_t sum = 0;
    uint64_t sumSquare = 0;
    double now = Simulator::Now().GetSeconds();
    uint32_t cnt = 0;
    for (uint32_t i = 0; i < N; i++) if (rxS1R1Bytes[i])
    {
        cnt++;
        rxS1R1Throughput << std::fixed << std::setprecision(3) << now << " " << i << ' ' 
                         << (rxS1R1Bytes[i] * 8) / measurementInterval_d / 1e6 << std::endl;
        sum += rxS1R1Bytes[i];
        sumSquare += rxS1R1Bytes[i] * rxS1R1Bytes[i];
        rxS1R1Bytes[i] = 0;
    }
    rxS1R1Throughput << std::fixed << std::setprecision(3) << now << " " << N << ' '
                     << (sum * 8) / measurementInterval_d / 1e6 / cnt << std::endl;

    bottlenetUtilization << std::fixed << std::setprecision(3) << now << " "
                         << sum * 8 / measurementInterval_d / bottleneckDataRate_n * 100
                         << std::endl;

    // calculate fairness only after extra flow started
    if (cnt == N)
    {
        double fairness = double(sum * sum) / (cnt * sumSquare);
        Jain_f << std::fixed << std::setprecision(3) << now << ' '
            << fairness << std::endl;
        if (fairness >= 0.95 && !flag95)
        {
            flag95 = true;
            std::cout << std::fixed << std::setprecision(3) << now << ' '
                    << fairness << std::endl;
        }
        if (fairness >= 0.99 && !flag99)
        {
            flag99 = true;
            std::cout << std::fixed << std::setprecision(3) << now << ' '
                    << fairness << std::endl;
        }
        if (fairness >= 0.999 && !flag999)
        {
            flag999 = true;
            std::cout << std::fixed << std::setprecision(3) << now << ' '
                    << fairness << std::endl;
        }
    }

    Simulator::Schedule(measurementInterval, &PrintThroughput);
}
void PrintQlen(Ptr<QueueDisc> qdisc)
{
    qlen_f << std::fixed << std::setprecision(3) << Simulator::Now().GetSeconds() << ' '
           << qdisc->GetNPackets() << std::endl;
    Simulator::Schedule(measurementInterval, &PrintQlen, qdisc);
}

int 
main(int argc, char* argv[]) 
{
    std::string config_path = "";  
    std::string outputFilePath = ".";
    std::string tcpTypeId = "TcpDctcp";
    std::string queueDiscTypeId = "RedQueueDisc";
    Time flowStartupWindow = Seconds (1);
    Time convergenceTime = Seconds (3);
    Time measurementWindow = Seconds (1);
    bool enableSwitchEcn = true;
    Time progressInterval = MilliSeconds (100);

    // Cebinae configurance
    bool enable_debug = 0;  
    Time dt {NanoSeconds (1048576)};
    Time vdt {NanoSeconds (1024)};
    Time l {NanoSeconds (65536)};
    uint32_t p {1};
    double tau {0.05};
    double delta_port {0.05};
    double delta_flow {0.05};

    CommandLine cmd (__FILE__);
    cmd.AddValue("config_path", "Path to the json configuration file", config_path);
    cmd.AddValue ("outputFilePath", "output file path", outputFilePath);
    cmd.AddValue ("tcpTypeId", "ns-3 TCP TypeId", tcpTypeId);
    cmd.AddValue ("flowStartupWindow", "startup time window (TCP staggered starts)", flowStartupWindow);
    cmd.AddValue ("convergenceTime", "convergence time", convergenceTime);
    cmd.AddValue ("measurementWindow", "measurement window", measurementWindow);
    cmd.AddValue ("enableSwitchEcn", "enable ECN at switches", enableSwitchEcn);
    cmd.AddValue ("queueDiscTypeId", "ns-3 QueueDisc TypeId", queueDiscTypeId);
    cmd.AddValue ("enable_debug", "Enable logging", enable_debug);
    cmd.AddValue ("dt", "CebinaeQueueDisc", dt);
    cmd.AddValue ("vdt", "CebinaeQueueDisc", vdt);
    cmd.AddValue ("l", "CebinaeQueueDisc", l);
    cmd.AddValue ("p", "CebinaeQueueDisc", p);
    cmd.AddValue ("tau", "CebinaeQueueDisc", tau);
    cmd.AddValue ("delta_port", "CebinaeQueueDisc", delta_port);
    cmd.AddValue ("delta_flow", "CebinaeQueueDisc", delta_flow);
    cmd.Parse (argc, argv);

    std::cout << "config_path: " << config_path << std::endl;
    std::cout << "outputFilePath: " << outputFilePath << std::endl;
    std::cout << "queueDiscTypeId: " << queueDiscTypeId << std::endl;

    // valid queueDisc config
    if (queueDiscTypeId != "RedQueueDisc" && queueDiscTypeId != "CebinaeQueueDisc") {
      std::cerr << "unsupported queueDisc: " << queueDiscTypeId << std::endl;
      return EXIT_FAILURE;
    }

    Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::" + tcpTypeId));

    Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1448));
    Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (2));
    GlobalValue::Bind ("ChecksumEnabled", BooleanValue (false));

    if (queueDiscTypeId == "RedQueueDisc") {
        // Set default parameters for RED queue disc
        Config::SetDefault ("ns3::RedQueueDisc::UseEcn", BooleanValue (enableSwitchEcn));
        // ARED may be used but the queueing delays will increase; it is disabled
        // here because the SIGCOMM paper did not mention it
        // Config::SetDefault ("ns3::RedQueueDisc::ARED", BooleanValue (true));
        // Config::SetDefault ("ns3::RedQueueDisc::Gentle", BooleanValue (true));
        Config::SetDefault ("ns3::RedQueueDisc::UseHardDrop", BooleanValue (false));
        Config::SetDefault ("ns3::RedQueueDisc::MeanPktSize", UintegerValue (1500));
        // Triumph and Scorpion switches used in DCTCP Paper have 4 MB of buffer
        // If every packet is 1500 bytes, 2666 packets can be stored in 4 MB
        Config::SetDefault ("ns3::RedQueueDisc::MaxSize", QueueSizeValue (QueueSize ("2666p")));
        // DCTCP tracks instantaneous queue length only; so set QW = 1
        Config::SetDefault ("ns3::RedQueueDisc::QW", DoubleValue (1));
        Config::SetDefault ("ns3::RedQueueDisc::MinTh", DoubleValue (20));
        Config::SetDefault ("ns3::RedQueueDisc::MaxTh", DoubleValue (60));
    }
    else {
        Config::SetDefault ("ns3::CebinaeQueueDisc::debug", BooleanValue (enable_debug));
        Config::SetDefault ("ns3::CebinaeQueueDisc::dT", TimeValue (dt));
        Config::SetDefault ("ns3::CebinaeQueueDisc::vdT", TimeValue (vdt));
        Config::SetDefault ("ns3::CebinaeQueueDisc::L", TimeValue (l));
        Config::SetDefault ("ns3::CebinaeQueueDisc::P", UintegerValue (p));
        Config::SetDefault ("ns3::CebinaeQueueDisc::tau", DoubleValue (tau));
        Config::SetDefault ("ns3::CebinaeQueueDisc::delta_port", DoubleValue (delta_port));
        Config::SetDefault ("ns3::CebinaeQueueDisc::delta_flow", DoubleValue (delta_flow));
        Config::SetDefault ("ns3::CebinaeQueueDisc::pool", BooleanValue (true));
        Config::SetDefault ("ns3::CebinaeQueueDisc::MaxSize", QueueSizeValue (QueueSize ("2666p")));

        Config::SetDefault ("ns3::CebinaeQueueDisc::enableECN", BooleanValue (true));
    }

    rxS1R1Bytes.resize(N, 0);

    NodeContainer S1;
    NodeContainer R1;
    Ptr<Node> T1 = CreateObject<Node>();
    Ptr<Node> T2 = CreateObject<Node>();
    S1.Create(N);
    R1.Create(N);

    // 1MB Tx and Rx buffer for large bandwidth
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(1 << 20));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(1 << 20));

    // DCTCP socket configuration
    Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue(2));
    Config::SetDefault("ns3::TcpSocketBase::UseEcn", EnumValue(TcpSocketState::On));
    // smaller RTO for high speed network
    Config::SetDefault("ns3::TcpSocketBase::MinRto", TimeValue(MilliSeconds(10)));

    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1448));
    GlobalValue::Bind("ChecksumEnabled", BooleanValue(false));

    Config::SetDefault("ns3::RedQueueDisc::UseEcn", BooleanValue(true));
    Config::SetDefault("ns3::RedQueueDisc::UseHardDrop", BooleanValue(false));
    Config::SetDefault("ns3::RedQueueDisc::MeanPktSize", UintegerValue(1500));
    Config::SetDefault("ns3::RedQueueDisc::MaxSize", QueueSizeValue(QueueSize("2666p")));
    Config::SetDefault("ns3::RedQueueDisc::QW", DoubleValue(1));
    Config::SetDefault("ns3::RedQueueDisc::MinTh", DoubleValue(50));
    Config::SetDefault("ns3::RedQueueDisc::MaxTh", DoubleValue(150));

    PointToPointHelper pointToPointSR;
    pointToPointSR.SetDeviceAttribute("DataRate", StringValue(FlowDataRate));
    pointToPointSR.SetChannelAttribute("Delay", StringValue("10us"));

    PointToPointHelper pointToPointT;
    pointToPointT.SetDeviceAttribute("DataRate", StringValue(bottleneckDataRate));
    pointToPointT.SetChannelAttribute("Delay", StringValue("10us"));

    std::vector<NetDeviceContainer> S1T1;
    std::vector<NetDeviceContainer> R1T2;
    S1T1.reserve(N);
    R1T2.reserve(N);
    for (uint32_t i = 0; i < N; i++)
    {
        Ptr<Node> n = S1.Get(i);
        S1T1.push_back(pointToPointSR.Install(n, T1));
    }
    for (uint32_t i = 0; i < N; i++)
    {
        Ptr<Node> n = R1.Get(i);
        R1T2.push_back(pointToPointSR.Install(n, T2));
    }
    NetDeviceContainer T1T2 = pointToPointT.Install(T1, T2);

    InternetStackHelper stack;
    stack.InstallAll();

    TrafficControlHelper tchRed0;
    if (queueDiscTypeId == "RedQueueDisc") {
        tchRed0.SetRootQueueDisc ("ns3::RedQueueDisc",
                           "LinkBandwidth", StringValue (bottleneckDataRate),
                           "LinkDelay", StringValue ("10us"),
                           "MinTh", DoubleValue (20),
                           "MaxTh", DoubleValue (60));
    }
    else if (queueDiscTypeId == "CebinaeQueueDisc") {
        tchRed0.SetRootQueueDisc ("ns3::CebinaeQueueDisc",
                           "DataRate", StringValue (bottleneckDataRate),
                           "MinTh", QueueSizeValue (QueueSize ("20p")),
                           "MaxTh", QueueSizeValue( QueueSize("60p")));
    }
    QueueDiscContainer queueDiscs0 = tchRed0.Install(T1T2);

    TrafficControlHelper tchRed1;
    if (queueDiscTypeId == "RedQueueDisc") {
        tchRed1.SetRootQueueDisc ("ns3::RedQueueDisc",
                           "LinkBandwidth", StringValue (FlowDataRate),
                           "LinkDelay", StringValue ("10us"),
                           "MinTh", DoubleValue (20),
                           "MaxTh", DoubleValue (60));
    }
    else if (queueDiscTypeId == "CebinaeQueueDisc") {
        tchRed1.SetRootQueueDisc ("ns3::CebinaeQueueDisc",
                           "DataRate", StringValue (FlowDataRate),
                           "MinTh", QueueSizeValue (QueueSize ("20p")),
                           "MaxTh", QueueSizeValue( QueueSize("60p")));
    }

    for (uint32_t i = 0; i < N; i++)
    {
        tchRed1.Install(S1T1[i].Get(1));
    }
    for (uint32_t i = 0; i < N; i++)
    {
        tchRed1.Install(R1T2[i].Get(1));
    }

    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> ipS1T1;
    std::vector<Ipv4InterfaceContainer> ipR1T2;
    ipS1T1.reserve(N);
    ipR1T2.reserve(N);
    address.SetBase("172.16.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ipT1T2 = address.Assign(T1T2);
    address.SetBase("10.1.1.0", "255.255.255.0");
    for (uint32_t i = 0; i < N; i++)
    {
        ipS1T1.push_back(address.Assign(S1T1[i]));
        address.NewNetwork();
    }
    address.SetBase("10.2.1.0", "255.255.255.0");
    for (uint32_t i = 0; i < N; i++)
    {
        ipR1T2.push_back(address.Assign(R1T2[i]));
        address.NewNetwork();
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    std::vector<Ptr<BulkSendApplication>> s1Clients;
    std::vector<Ptr<PacketSink>> r1Sinks;
    s1Clients.reserve(N);
    r1Sinks.reserve(N);
    for (uint32_t i = 0; i < N; i++)
    {
        uint16_t port = 50000 + i;
        Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);

        ApplicationContainer sinkApp = sinkHelper.Install(R1.Get(i));
        sinkApp.Start(startTime);
        sinkApp.Stop(stopTime);

        Ptr<PacketSink> packetSink = sinkApp.Get(0)->GetObject<PacketSink>();
        r1Sinks.push_back(packetSink);

        BulkSendHelper clientHelper1("ns3::TcpSocketFactory", Address());
        clientHelper1.SetAttribute("MaxBytes", UintegerValue(0));

        AddressValue remoteAddress(InetSocketAddress(ipR1T2[i].GetAddress(0), port));
        clientHelper1.SetAttribute("Remote", remoteAddress);

        ApplicationContainer clientApps1 = clientHelper1.Install(S1.Get(i));
        Time flowStartTime = i < N - 1 ? Seconds(0.001 * i) : extraFlowStartTime;
        clientApps1.Start(flowStartTime);
        clientApps1.Stop(stopTime);        

        Ptr<BulkSendApplication> client = clientApps1.Get(0)->GetObject<BulkSendApplication>();
        s1Clients.push_back(client);
    }

    Simulator::Schedule(startTime, &PrintProgress);

    for (uint32_t i = 0; i < N; i++)
    {
        s1Clients[i]->TraceConnectWithoutContext("Tx", MakeBoundCallback(&TraceSender, 
                                                                         S1.Get(i)->GetId()));
        r1Sinks[i]->TraceConnectWithoutContext("Rx", MakeBoundCallback(&TraceSink, i));
    }

    if(system(("mkdir -p " + outputFilePath).c_str()) != 0) {
        std::cerr << "Error creating output directory" << std::endl;
        return EXIT_FAILURE;
    }
    rxS1R1Throughput.open(outputFilePath + "S1R1-throughput.dat", std::ios::out);
    bottlenetUtilization.open(outputFilePath + "bottlenet-utilization.dat", std::ios::out);
    Jain_f.open(outputFilePath + "Jain.dat", std::ios::out);
    rxS1R1Throughput << "#Time(s)" << "\t" << "throughput(Mbps)" << std::endl;
    bottlenetUtilization << "#Time(s)" << "\t" << "utilization(%)" << std::endl;
    Jain_f << "#Time(s)" << '\t' << "Jain's" << std::endl;
    Simulator::Schedule(measurementInterval, &PrintThroughput);

    // print queue length at T1
    qlen_f.open(outputFilePath + "qlen.dat", std::ios::out);
    qlen_f << "#Time(s)" << "\t" << "Qlen(p)" << std::endl;
    Simulator::Schedule(measurementInterval, &PrintQlen, queueDiscs0.Get(0));
                                  
    Simulator::Stop(stopTime + TimeStep(1));
    Simulator::Run();

    rxS1R1Throughput.close();
    bottlenetUtilization.close();
    qlen_f.close();
    Jain_f.close();
    Simulator::Destroy();
    return 0;
}
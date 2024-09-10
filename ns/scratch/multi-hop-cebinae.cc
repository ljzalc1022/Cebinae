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

// The topology is roughly as follows
//
//  S1         S3
//  |           |  (1 Gbps)
//  T1 ------- T2 -- R1
//  |           |  (1 Gbps)
//  S2         R2
//
// The link between switch T1 and T2 is 10 Gbps.  All other
// links are 1 Gbps.  In the SIGCOMM paper, there is a Scorpion switch
// between T1 and T2, but it doesn't contribute another bottleneck.
//
// S1 and S3 each have 10 senders sending to receiver R1 (20 total)
// S2 (20 senders) sends traffic to R2 (20 receivers)

std::ofstream rxS1R1Thpt;
std::ofstream rxS2R2Thpt;
std::ofstream rxS3R1Thpt;
std::ofstream fairnessIndex;
std::ofstream txS1R1Thpt;
std::ofstream txS2R2Thpt;
std::ofstream txS3R1Thpt;

std::vector<uint64_t> rxS1R1Bytes;
std::vector<uint64_t> rxS2R2Bytes;
std::vector<uint64_t> rxS3R1Bytes;
std::vector<uint64_t> txS1R1Bytes;
std::vector<uint64_t> txS2R2Bytes;
std::vector<uint64_t> txS3R1Bytes;


void
PrintProgress(Time interval)
{
    std::cout << "Progress to " << std::fixed << std::setprecision(1)
              << Simulator::Now().GetSeconds() << " seconds simulation time" << std::endl;
    Simulator::Schedule(interval, &PrintProgress, interval);
}

void
TraceS1R1Sink(std::size_t index, Ptr<const Packet> p, const Address& a)
{
    rxS1R1Bytes[index] += p->GetSize();
}

void
TraceS2R2Sink(std::size_t index, Ptr<const Packet> p, const Address& a)
{
    rxS2R2Bytes[index] += p->GetSize();
}

void
TraceS3R1Sink(std::size_t index, Ptr<const Packet> p, const Address& a)
{
    rxS3R1Bytes[index] += p->GetSize();
}

void
TraceS1R1Send(std::size_t index, Ptr<const Packet> p)
{
    txS1R1Bytes[index] += p->GetSize();
}

void
TraceS2R2Send(std::size_t index, Ptr<const Packet> p)
{
    txS2R2Bytes[index] += p->GetSize();
}

void
TraceS3R1Send(std::size_t index, Ptr<const Packet> p)
{
    txS3R1Bytes[index] += p->GetSize();
}

void
InitializeCounters()
{
    
    for (std::size_t i = 0; i < 10; i++)
    {
        rxS1R1Bytes[i] = 0;
    }
    for (std::size_t i = 0; i < 20; i++)
    {
        rxS2R2Bytes[i] = 0;
    }
    for (std::size_t i = 0; i < 10; i++)
    {
        rxS3R1Bytes[i] = 0;
    }

    for (std::size_t i = 0; i < 10; i++)
    {
        txS1R1Bytes[i] = 0;
    }

    for (std::size_t i = 0; i < 20; i++)
    {
        txS2R2Bytes[i] = 0;
    }

    for (std::size_t i = 0; i < 10; i++)
    {
        txS3R1Bytes[i] = 0;
    }
}

void
PrintThroughput(Time measurementWindow)
{
    for (std::size_t i = 0; i < 10; i++)
    {
        rxS1R1Thpt << Simulator::Now().GetSeconds() << "s " << i << " "
                         << (rxS1R1Bytes[i] * 8) / (measurementWindow.GetSeconds()) / 1e6
                         << std::endl;
    }
    for (std::size_t i = 0; i < 20; i++)
    {
        rxS2R2Thpt << Simulator::Now().GetSeconds() << "s " << i << " "
                         << (rxS2R2Bytes[i] * 8) / (measurementWindow.GetSeconds()) / 1e6
                         << std::endl;
    }
    for (std::size_t i = 0; i < 10; i++)
    {
        rxS3R1Thpt << Simulator::Now().GetSeconds() << "s " << i << " "
                         << (rxS3R1Bytes[i] * 8) / (measurementWindow.GetSeconds()) / 1e6
                         << std::endl;
    }
    for (std::size_t i = 0; i < 10; i++)
    {
        txS1R1Thpt << Simulator::Now().GetSeconds() << "s " << i << " "
                         << (txS1R1Bytes[i] * 8) / (measurementWindow.GetSeconds()) / 1e6
                         << std::endl;
    }
    for (std::size_t i = 0; i < 20; i++)
    {
        txS2R2Thpt << Simulator::Now().GetSeconds() << "s " << i << " "
                         << (txS2R2Bytes[i] * 8) / (measurementWindow.GetSeconds()) / 1e6
                         << std::endl;
    }
    for (std::size_t i = 0; i < 10; i++)
    {
        txS3R1Thpt << Simulator::Now().GetSeconds() << "s " << i << " "
                         << (txS3R1Bytes[i] * 8) / (measurementWindow.GetSeconds()) / 1e6
                         << std::endl;
    }

    // Print Fairness
    double average = 0;
    uint64_t sumSquares = 0;
    uint64_t sum = 0;
    double idealBytes, x, sum_x = 0, sumSquare_x = 0;
    double fairness = 0;
    idealBytes = 50 * measurementWindow.GetSeconds() * 1e6 / 8;
    for (std::size_t i = 0; i < 10; i++)
    {
        sum += txS1R1Bytes[i];
        sumSquares += (txS1R1Bytes[i] * txS1R1Bytes[i]);
        x = txS1R1Bytes[i] / idealBytes;
        sum_x += x;
        sumSquare_x += x * x;

    }
    average = ((sum / 10) * 8 / measurementWindow.GetSeconds()) / 1e6;
    fairness = static_cast<double>(sum * sum) / (10 * sumSquares);
    fairnessIndex << "Average throughput for S1-R1 flows: " << std::fixed << std::setprecision(2)
                  << average << " Mbps; fairness: " << std::fixed << std::setprecision(3)
                  << fairness << std::endl;
    average = 0;
    sumSquares = 0;
    sum = 0;
    fairness = 0;
    idealBytes = 475 * measurementWindow.GetSeconds() * 1e6 / 8;
    for (std::size_t i = 0; i < 20; i++)
    {
        sum += txS2R2Bytes[i];
        sumSquares += (txS2R2Bytes[i] * txS2R2Bytes[i]);
        x = txS2R2Bytes[i] / idealBytes;
        sum_x += x;
        sumSquare_x += x * x;
    }
    average = ((sum / 20) * 8 / measurementWindow.GetSeconds()) / 1e6;
    fairness = static_cast<double>(sum * sum) / (20 * sumSquares);
    fairnessIndex << "Average throughput for S2-R2 flows: " << std::fixed << std::setprecision(2)
                  << average << " Mbps; fairness: " << std::fixed << std::setprecision(3)
                  << fairness << std::endl;
    average = 0;
    sumSquares = 0;
    sum = 0;
    fairness = 0;
    idealBytes = 50 * measurementWindow.GetSeconds() * 1e6 / 8;
    for (std::size_t i = 0; i < 10; i++)
    {
        sum += txS3R1Bytes[i];
        sumSquares += (txS3R1Bytes[i] * txS3R1Bytes[i]);
        x = txS3R1Bytes[i] / idealBytes;
        sum_x += x;
        sumSquare_x += x * x;
    }
    average = ((sum / 10) * 8 / measurementWindow.GetSeconds()) / 1e6;
    fairness = static_cast<double>(sum * sum) / (10 * sumSquares);
    fairnessIndex << "Average throughput for S3-R1 flows: " << std::fixed << std::setprecision(2)
                  << average << " Mbps; fairness: " << std::fixed << std::setprecision(3)
                  << fairness << std::endl;

    fairnessIndex << "Global Fairness Index " << sum_x * sum_x / 40 / sumSquare_x << std::endl;;
    sum = 0;
    for (std::size_t i = 0; i < 10; i++)
    {
        sum += txS1R1Bytes[i];
    }
    for (std::size_t i = 0; i < 20; i++)
    {
        sum += txS2R2Bytes[i];
    }
    fairnessIndex << "Aggregate user-level throughput for flows through T1: "
                  << static_cast<double>(sum * 8) / 1e9 << " Gbps" << std::endl;
    sum = 0;
    for (std::size_t i = 0; i < 10; i++)
    {
        sum += txS3R1Bytes[i];
    }
    for (std::size_t i = 0; i < 10; i++)
    {
        sum += txS1R1Bytes[i];
    }
    fairnessIndex << "Aggregate user-level throughput for flows to R1: "
                  << static_cast<double>(sum * 8) / 1e9 << " Gbps" << std::endl;

    for (int i = 0; i < 10; i++) {
        txS1R1Bytes[i] = 0;
        rxS1R1Bytes[i] = 0;
    }

    for (int i = 0; i < 20; i++) {
        txS2R2Bytes[i] = 0;
        rxS2R2Bytes[i] = 0;
    }

    for (int i = 0; i < 10; i++) {
        txS3R1Bytes[i] = 0;
        rxS3R1Bytes[i] = 0;
    }
    Simulator::Schedule(measurementWindow, &PrintThroughput, measurementWindow);
}

int
main(int argc, char* argv[])
{
    std::string config_path = "";  
    std::string outputFilePath = ".";
    std::string tcpTypeId = "TcpDctcp";
    std::string queueDiscTypeId = "RedQueueDisc";
    bool enableSwitchEcn = true;

    Time flowStartupWindow = Seconds(1);
    Time convergenceTime = Seconds(3);
    Time measurementWindow = Seconds(1);
    Time measurementInterval = MilliSeconds(10);
    Time progressInterval = MilliSeconds(100);

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

    Time startTime = Seconds(0);
    Time stopTime = flowStartupWindow + convergenceTime + measurementWindow;
    Time clientStartTime = startTime;

    rxS1R1Bytes.reserve(10);
    rxS2R2Bytes.reserve(20);
    rxS3R1Bytes.reserve(10);
    txS1R1Bytes.reserve(10);
    txS2R2Bytes.reserve(20);
    txS3R1Bytes.reserve(10);

    NodeContainer S1;
    NodeContainer S2;
    NodeContainer S3;
    NodeContainer R2;
    Ptr<Node> T1 = CreateObject<Node>();
    Ptr<Node> T2 = CreateObject<Node>();
    Ptr<Node> R1 = CreateObject<Node>();
    S1.Create(10);
    S2.Create(20);
    S3.Create(10);
    R2.Create(20);

    PointToPointHelper pointToPointSR;
    pointToPointSR.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    pointToPointSR.SetChannelAttribute("Delay", StringValue ("10us"));

    PointToPointHelper pointToPointT;
    pointToPointT.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
    pointToPointT.SetChannelAttribute("Delay", StringValue ("10us"));

    // Create a total of 62 links.
    std::vector<NetDeviceContainer> S1T1;
    S1T1.reserve(10);
    std::vector<NetDeviceContainer> S2T1;
    S2T1.reserve(20);
    std::vector<NetDeviceContainer> S3T2;
    S3T2.reserve(10);
    std::vector<NetDeviceContainer> R2T2;
    R2T2.reserve(20);
    NetDeviceContainer T1T2 = pointToPointT.Install(T1, T2);
    NetDeviceContainer R1T2 = pointToPointSR.Install(R1, T2);

    for (std::size_t i = 0; i < 10; i++)
    {
        Ptr<Node> n = S1.Get(i);
        S1T1.push_back(pointToPointSR.Install(n, T1));
    }
    for (std::size_t i = 0; i < 20; i++)
    {
        Ptr<Node> n = S2.Get(i);
        S2T1.push_back(pointToPointSR.Install(n, T1));
    }
    for (std::size_t i = 0; i < 10; i++)
    {
        Ptr<Node> n = S3.Get(i);
        S3T2.push_back(pointToPointSR.Install(n, T2));
    }
    for (std::size_t i = 0; i < 20; i++)
    {
        Ptr<Node> n = R2.Get(i);
        R2T2.push_back(pointToPointSR.Install(n, T2));
    }

    InternetStackHelper stack;
    stack.InstallAll();

    TrafficControlHelper tchRed10;
    // MinTh = 50, MaxTh = 150 recommended in ACM SIGCOMM 2010 DCTCP Paper
    // This yields a target (MinTh) queue depth of 60us at 10 Gb/s
    if (queueDiscTypeId == "RedQueueDisc") {
        tchRed10.SetRootQueueDisc ("ns3::RedQueueDisc",
                            "LinkBandwidth", StringValue ("10Gbps"),
                            "LinkDelay", StringValue ("10us"),
                            "MinTh", DoubleValue (50),
                            "MaxTh", DoubleValue (150));
    }
    else if (queueDiscTypeId == "CebinaeQueueDisc") {
        tchRed10.SetRootQueueDisc ("ns3::CebinaeQueueDisc",
                            "DataRate", StringValue ("10Gbps"),
                            "MinTh", QueueSizeValue (QueueSize ("50p")),
                            "MaxTh", QueueSizeValue( QueueSize("150p")));
    }
    QueueDiscContainer queueDiscs1 = tchRed10.Install(T1T2);

    TrafficControlHelper tchRed1;
    // MinTh = 20, MaxTh = 60 recommended in ACM SIGCOMM 2010 DCTCP Paper
    // This yields a target queue depth of 250us at 1 Gb/s
    if (queueDiscTypeId == "RedQueueDisc") {
        tchRed1.SetRootQueueDisc ("ns3::RedQueueDisc",
                           "LinkBandwidth", StringValue ("1Gbps"),
                           "LinkDelay", StringValue ("10us"),
                           "MinTh", DoubleValue (20),
                           "MaxTh", DoubleValue (60));
    }
    else if (queueDiscTypeId == "CebinaeQueueDisc") {
        tchRed1.SetRootQueueDisc ("ns3::CebinaeQueueDisc",
                           "DataRate", StringValue ("1Gbps"),
                           "MinTh", QueueSizeValue (QueueSize ("20p")),
                           "MaxTh", QueueSizeValue( QueueSize("60p")));
    }
    QueueDiscContainer queueDiscs2 = tchRed1.Install(R1T2.Get(1));
    for (std::size_t i = 0; i < 10; i++)
    {
        tchRed1.Install(S1T1[i].Get(1));
    }
    for (std::size_t i = 0; i < 20; i++)
    {
        tchRed1.Install(S2T1[i].Get(1));
    }
    for (std::size_t i = 0; i < 10; i++)
    {
        tchRed1.Install(S3T2[i].Get(1));
    }
    for (std::size_t i = 0; i < 20; i++)
    {
        tchRed1.Install(R2T2[i].Get(1));
    }

    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> ipS1T1;
    ipS1T1.reserve(10);
    std::vector<Ipv4InterfaceContainer> ipS2T1;
    ipS2T1.reserve(20);
    std::vector<Ipv4InterfaceContainer> ipS3T2;
    ipS3T2.reserve(10);
    std::vector<Ipv4InterfaceContainer> ipR2T2;
    ipR2T2.reserve(20);
    address.SetBase("172.16.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ipT1T2 = address.Assign(T1T2);
    address.SetBase("192.168.0.0", "255.255.255.0");
    Ipv4InterfaceContainer ipR1T2 = address.Assign(R1T2);
    address.SetBase("10.1.1.0", "255.255.255.0");
    for (std::size_t i = 0; i < 10; i++)
    {
        ipS1T1.push_back(address.Assign(S1T1[i]));
        address.NewNetwork();
    }
    address.SetBase("10.2.1.0", "255.255.255.0");
    for (std::size_t i = 0; i < 20; i++)
    {
        ipS2T1.push_back(address.Assign(S2T1[i]));
        address.NewNetwork();
    }
    address.SetBase("10.3.1.0", "255.255.255.0");
    for (std::size_t i = 0; i < 10; i++)
    {
        ipS3T2.push_back(address.Assign(S3T2[i]));
        address.NewNetwork();
    }
    address.SetBase("10.4.1.0", "255.255.255.0");
    for (std::size_t i = 0; i < 20; i++)
    {
        ipR2T2.push_back(address.Assign(R2T2[i]));
        address.NewNetwork();
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Each sender in S2 sends to a receiver in R2
    std::vector<Ptr<PacketSink>> r2Sinks;
    std::vector<Ptr<BulkSendApplication>> s2Senders;
    r2Sinks.reserve(20);
    s2Senders.reserve(20);
    for (std::size_t i = 0; i < 20; i++)
    {
        uint16_t port = 50000 + i;
        Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
        ApplicationContainer sinkApp = sinkHelper.Install(R2.Get(i));
        Ptr<PacketSink> packetSink = sinkApp.Get(0)->GetObject<PacketSink>();
        r2Sinks.push_back(packetSink);
        sinkApp.Start(startTime);
        sinkApp.Stop(stopTime);

        BulkSendHelper clientHelper1("ns3::TcpSocketFactory", Address());
        clientHelper1.SetAttribute("MaxBytes", UintegerValue(0));

        ApplicationContainer clientApps1;
        AddressValue remoteAddress(InetSocketAddress(ipR2T2[i].GetAddress(0), port));
        clientHelper1.SetAttribute("Remote", remoteAddress);
        clientApps1.Add(clientHelper1.Install(S2.Get(i)));
        Ptr<BulkSendApplication> bulkSend = clientApps1.Get(0)->GetObject<BulkSendApplication>();
        s2Senders.push_back(bulkSend);
        clientApps1.Start(i * flowStartupWindow / 20 + clientStartTime + MilliSeconds(i * 5));
        clientApps1.Stop(stopTime);
    }

    // Each sender in S1 and S3 sends to R1
    std::vector<Ptr<PacketSink>> s1r1Sinks;
    std::vector<Ptr<PacketSink>> s3r1Sinks;
    std::vector<Ptr<BulkSendApplication>> s1r1Senders;
    std::vector<Ptr<BulkSendApplication>> s3r1Senders;
    s1r1Sinks.reserve(10);
    s3r1Sinks.reserve(10);
    s1r1Senders.reserve(10);
    s3r1Senders.reserve(10);
    for (std::size_t i = 0; i < 20; i++)
    {
        uint16_t port = 50000 + i;
        Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
        ApplicationContainer sinkApp = sinkHelper.Install(R1);
        Ptr<PacketSink> packetSink = sinkApp.Get(0)->GetObject<PacketSink>();
        if (i < 10)
        {
            s1r1Sinks.push_back(packetSink);
        }
        else
        {
            s3r1Sinks.push_back(packetSink);
        }
        sinkApp.Start(startTime);
        sinkApp.Stop(stopTime);

        BulkSendHelper clientHelper1("ns3::TcpSocketFactory", Address());
        clientHelper1.SetAttribute("MaxBytes", UintegerValue(0));

        ApplicationContainer clientApps1;
        AddressValue remoteAddress(InetSocketAddress(ipR1T2.GetAddress(0), port));
        clientHelper1.SetAttribute("Remote", remoteAddress);
        if (i < 10)
        {
            clientApps1 = clientHelper1.Install(S1.Get(i));
            Ptr<BulkSendApplication> bulksend = clientApps1.Get(0)->GetObject<BulkSendApplication>();
            s1r1Senders.push_back(bulksend);
            clientApps1.Start(i * flowStartupWindow / 10 + clientStartTime + MilliSeconds(i * 5));
        }
        else
        {
            clientApps1 = clientHelper1.Install(S3.Get(i - 10));
            Ptr<BulkSendApplication> bulksend = clientApps1.Get(0)->GetObject<BulkSendApplication>();
            s3r1Senders.push_back(bulksend);
            clientApps1.Start((i - 10) * flowStartupWindow / 10 + clientStartTime +
                              MilliSeconds(i * 5));
        }

        clientApps1.Stop(stopTime);
    }

    if (system(("mkdir -p " + outputFilePath).c_str()) != 0)
    {
        std::cerr << "Failed to create output directory" << std::endl;
        return EXIT_FAILURE;
    }
    rxS1R1Thpt.open(outputFilePath + "dctcp-example-s1-r1-goodput.dat", std::ios::out);
    rxS1R1Thpt << "#Time(s) flow thruput(Mb/s)" << std::endl;
    rxS2R2Thpt.open(outputFilePath + "dctcp-example-s2-r2-goodput.dat", std::ios::out);
    rxS2R2Thpt << "#Time(s) flow thruput(Mb/s)" << std::endl;
    rxS3R1Thpt.open(outputFilePath + "dctcp-example-s3-r1-goodput.dat", std::ios::out);
    rxS3R1Thpt << "#Time(s) flow thruput(Mb/s)" << std::endl;
    fairnessIndex.open(outputFilePath + "dctcp-example-fairness.dat", std::ios::out);
    txS1R1Thpt.open(outputFilePath + "dctcp-example-s1-r1-throughput.dat", std::ios::out);
    txS1R1Thpt << "#Time(s) flow thruput(Mb/s)" << std::endl;
    txS2R2Thpt.open(outputFilePath + "dctcp-example-s2-r2-throughput.dat", std::ios::out);
    txS2R2Thpt << "#Time(s) flow thruput(Mb/s)" << std::endl;
    txS3R1Thpt.open(outputFilePath + "dctcp-example-s3-r1-throughput.dat", std::ios::out);
    txS3R1Thpt << "#Time(s) flow thruput(Mb/s)" << std::endl;
    for (std::size_t i = 0; i < 10; i++)
    {
        s1r1Senders[i]->TraceConnectWithoutContext("Tx", MakeBoundCallback(&TraceS1R1Send, i));
        s1r1Sinks[i]->TraceConnectWithoutContext("Rx", MakeBoundCallback(&TraceS1R1Sink, i));
    }
    for (std::size_t i = 0; i < 20; i++)
    {
        s2Senders[i]->TraceConnectWithoutContext("Tx", MakeBoundCallback(&TraceS2R2Send, i));
        r2Sinks[i]->TraceConnectWithoutContext("Rx", MakeBoundCallback(&TraceS2R2Sink, i));
    }
    for (std::size_t i = 0; i < 10; i++)
    {
        s3r1Senders[i]->TraceConnectWithoutContext("Tx", MakeBoundCallback(&TraceS3R1Send, i));
        s3r1Sinks[i]->TraceConnectWithoutContext("Rx", MakeBoundCallback(&TraceS3R1Sink, i));
    }
    Simulator::Schedule(startTime, &InitializeCounters);
    Simulator::Schedule(measurementInterval, &PrintThroughput, measurementInterval);
    Simulator::Schedule(progressInterval, &PrintProgress, progressInterval);
    Simulator::Stop(stopTime + TimeStep(1));

    Simulator::Run();

    rxS1R1Thpt.close();
    rxS2R2Thpt.close();
    rxS3R1Thpt.close();
    fairnessIndex.close();
    Simulator::Destroy();
    return 0;
}
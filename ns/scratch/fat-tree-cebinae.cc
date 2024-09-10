#include <cassert>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <unordered_map>
#include <vector>

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"

#include "ns3/tcp-socket-base.h"

using namespace ns3;
using namespace std::chrono;

NS_LOG_COMPONENT_DEFINE("fat-tree-cebinae");

Time progressInterval = MilliSeconds(100);
/**
 * \brief print progress during simulation
 */
void 
printProgress()
{
    std::cout << "Progress to " << std::fixed << std::setprecision(1)
              << Simulator::Now().GetSeconds() << " seconds simulation time" << std::endl;
    Simulator::Schedule(progressInterval, &printProgress);
}

std::vector<Ptr<QueueDisc>> ToR_switches; 
std::ofstream qlen_f;
Time qlen_print_interval = MilliSeconds(1);
void
PrintQlen()
{
    for(size_t i = 0; i < ToR_switches.size(); i++)
    {
        uint32_t qlen = ToR_switches[i]->GetNPackets();
        qlen_f << std::fixed << std::setprecision(3) << Simulator::Now().GetSeconds() << ' '
               << i << ' ' << qlen << std::endl;
    }
    Simulator::Schedule(qlen_print_interval, &PrintQlen);
}

/**
 * \brief return the IP of the server
 */
inline Ipv4Address
node_id_to_IP(uint32_t id)
{
    return Ipv4Address(0x0b000001 + id * 256);
}

std::ofstream FCT_f;
std::vector<uint64_t> flow_size;
std::vector<Time> flow_start_time;
std::vector<Time> flow_end_time;
void record_FCT(uint32_t flow_id, 
                const Ptr<const Packet> packet, 
                const TcpHeader &header, 
                const Ptr<const TcpSocketBase> socket)
{
    flow_end_time[flow_id] = Simulator::Now();
}
void cal_FCT(uint32_t flow_num)
{
    for (uint32_t i = 0; i < flow_num; ++i)
    {
        double FCT = (flow_end_time[i] - flow_start_time[i]).GetMicroSeconds();
        if(FCT < 0)
        {
            std::cout << i << ' ' << flow_size[i] << ' '
                      << flow_start_time[i].GetMicroSeconds() << ' '
                      << flow_end_time[i].GetMicroSeconds() << std::endl;
        }
        FCT_f << flow_size[i] << ' ';
        FCT_f << FCT << std::endl;
    }
}

uint64_t totalAck{ 0 };
uint64_t totalAckBigflow{ 0 };
void TraceBigflow(bool withBigflow)
{
    totalAck++;
    if (withBigflow) totalAckBigflow++;
}

void trace_socket(Ptr<BulkSendApplication> app, uint32_t flow_id)
{
    app->GetSocket()->TraceConnectWithoutContext("Rx", MakeBoundCallback(&record_FCT, flow_id));
    app->GetSocket()->TraceConnectWithoutContext("AckRxWithBigflow", MakeCallback(&TraceBigflow));
}

// capture L2 packets 
// static void 
// PcapPhy(Ptr<PcapFileWrapper> file, Ptr<const Packet> p)
// {
//     NS_LOG_DEBUG("PcapPhy");
//     file->Write(Simulator::Now(), p);
// }

int 
main(int argc, char *argv[])
{
    std::string flow_file = "websearch-30-25G.txt"; // name of the traffic trace
    std::string topology_file = "fat-tree.txt";
    
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
    cmd.AddValue ("topology_file", "topology file path", topology_file);
    cmd.AddValue ("flow_file", "flow file path", flow_file);
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


    // simulation event time
    const Time start_time = Seconds(0);
    const Time app_start_time = Seconds(2);
    const Time stop_time = Seconds(5);

    // input and output file names
    const std::string FCT_file = outputFilePath + "fct.dat";
    const std::string qlen_file = outputFilePath + "qlen.dat";
    if (system(("mkdir -p " + outputFilePath).c_str()) != 0)
    {
        std::cerr << "Failed to create output directory" << std::endl;
        return EXIT_FAILURE;
    }

    // config for sockets
    Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::" + tcpTypeId));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1448));
    Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue(2));
    // 1MB Tx and Rx buffer for large bandwidth
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(1 << 20));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(1 << 20));    
    GlobalValue::Bind("ChecksumEnabled", BooleanValue(false));
    
    // config for RED to support DCTCP
    Config::SetDefault("ns3::RedQueueDisc::UseEcn", BooleanValue(true));
    Config::SetDefault("ns3::RedQueueDisc::UseHardDrop", BooleanValue(false));
    Config::SetDefault("ns3::RedQueueDisc::MeanPktSize", UintegerValue(1500));
    // 4MB buffer for 1500 bytes Packets
    Config::SetDefault("ns3::RedQueueDisc::MaxSize", 
                       QueueSizeValue(QueueSize("2666p"))); 
    Config::SetDefault("ns3::RedQueueDisc::QW", DoubleValue(1));

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


    // Create nodes
    uint32_t node_num, link_num;
    std::ifstream topo_f;    
    topo_f.open(topology_file, std::ios::in);
    topo_f >> node_num >> link_num;
    NodeContainer nodes;
    nodes.Create(node_num);

    // read in node types, see fat-tree-gen.py for more infomation
    std::vector<int> node_type(node_num);
    for (uint32_t i = 0; i < node_num; i++)
    {
        topo_f >> node_type[i];
    }

    // install internet stack on all nodes
    InternetStackHelper stack;
    stack.InstallAll();

    // Assign IP to servers
    std::vector<Ipv4Address> server_address(node_num);
    for (uint32_t i = 0; i < node_num; i++)
    {
        if(!node_type[i])
        {
            server_address[i] = node_id_to_IP(i);
        }
    }

    // Create links
    for (uint32_t i = 0; i < link_num; i++)
    {
        // read link info from the file
        uint32_t src;
        uint32_t dst;
        std::string data_rate;
        std::string link_delay;
        topo_f >> src >> dst >> data_rate >> link_delay;
        
        Ptr<Node> s_node = nodes.Get(src);
        Ptr<Node> d_node = nodes.Get(dst);
        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue(data_rate));
        p2p.SetChannelAttribute("Delay", StringValue(link_delay));

        NetDeviceContainer devices = p2p.Install(s_node, d_node);
        // config switch qdisc before ip address is allocated
        TrafficControlHelper tchRed;
        if (queueDiscTypeId == "RedQueueDisc") {
            tchRed.SetRootQueueDisc ("ns3::RedQueueDisc",
                            "LinkBandwidth", StringValue ("10Gbps"),
                            "LinkDelay", StringValue ("10us"),
                            "MinTh", DoubleValue (50),
                            "MaxTh", DoubleValue (150));
        }
        else if (queueDiscTypeId == "CebinaeQueueDisc") {
            tchRed.SetRootQueueDisc ("ns3::CebinaeQueueDisc",
                            "DataRate", StringValue ("10Gbps"),
                            "MinTh", QueueSizeValue (QueueSize ("50p")),
                            "MaxTh", QueueSizeValue( QueueSize("150p")));
        }

        if(node_type[src]) 
        {
            QueueDiscContainer qdiscs = tchRed.Install(devices.Get(0));
            if(node_type[src] == 1)
            {
                ToR_switches.push_back(qdiscs.Get(0));
            }
        }
        if(node_type[dst]) 
        {
            QueueDiscContainer qdiscs = tchRed.Install(devices.Get(1));
            if(node_type[dst] == 1)
            {
                ToR_switches.push_back(qdiscs.Get(0));
            }
        }
        // --- from HPCC remark
		// Assigne server IP
		// Note: this should be before the automatic assignment below (ipv4.Assign(d)),
		// because we want our IP to be the primary IP (first in the IP address list),
		// so that the global routing is based on our IP
        if(!node_type[src])
        {
            Ptr<Ipv4> ipv4 = s_node->GetObject<Ipv4>();
            uint32_t id = ipv4->AddInterface(devices.Get(0));
            NS_LOG_DEBUG(src << ' ' << id);
            ipv4->AddAddress(id, 
                             Ipv4InterfaceAddress(server_address[src], 
                                                  Ipv4Mask(0xff000000)));
        }
        if(!node_type[dst])
        {
            Ptr<Ipv4> ipv4 = d_node->GetObject<Ipv4>();
            uint32_t id = ipv4->AddInterface(devices.Get(1));
            NS_LOG_DEBUG(dst << ' ' << id);
            ipv4->AddAddress(id, 
                             Ipv4InterfaceAddress(server_address[dst], 
                                                  Ipv4Mask(0xff000000)));
        }
        // --- from HPCC remark        
		// This is just to set up the connectivity between nodes. 
        // The IP addresses are useless
        char ip_string[20];
        sprintf(ip_string, "10.%d.%d.0", i / 256 + 100, i % 256);
        Ipv4AddressHelper ipv4;
        ipv4.SetBase(ip_string, "255.255.255.0");
        ipv4.Assign(devices);
    }
    Config::SetDefault("ns3::Ipv4GlobalRouting::RandomEcmpRouting", 
                       BooleanValue(true));
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // port starting from 10000
    std::unordered_map<uint32_t, uint16_t> port_pool;
    for (uint32_t i = 0; i < node_num; i++) if(!node_type[i])
    {
        port_pool[i] = 10000;
    }

    // read in the flow trace
    std::ifstream flow_f;
    uint32_t flow_num;
    flow_f.open(flow_file, std::ios::in);
    flow_f >> flow_num;
    flow_size.reserve(flow_num);
    flow_start_time.reserve(flow_num);
    flow_end_time.reserve(flow_num);
    for (uint32_t i = 0; i < flow_num; i++)
    {
        uint32_t src;
        uint32_t dst;
        uint32_t pg; // pg = 3
        uint16_t src_port;
        uint16_t dst_port;
        double _flow_start_time;
        flow_f >> src >> dst >> pg >> dst_port >> flow_size[i] >> _flow_start_time;
        src_port = port_pool[src]++;
        dst_port = port_pool[dst]++;
        flow_start_time[i] = Seconds(_flow_start_time);

        // install sink app on dst
        Address sink_local_address(InetSocketAddress(Ipv4Address::GetAny(), dst_port));
        PacketSinkHelper sink_helper("ns3::TcpSocketFactory", sink_local_address);
        ApplicationContainer sink_app = sink_helper.Install(nodes.Get(dst));
        sink_app.Start(start_time);
        sink_app.Stop(stop_time);

        // install bulksend app on src
        Address blk_local_address(InetSocketAddress(Ipv4Address::GetAny(), src_port));
        AddressValue remoteAddress(InetSocketAddress(server_address[dst], dst_port));
        BulkSendHelper blk_helper("ns3::TcpSocketFactory", blk_local_address);
        blk_helper.SetAttribute("MaxBytes", UintegerValue(flow_size[i]));
        blk_helper.SetAttribute("Remote", remoteAddress);
        ApplicationContainer blk_app = blk_helper.Install(nodes.Get(src));
        blk_app.Start(flow_start_time[i]);
        blk_app.Stop(stop_time);

        Simulator::Schedule(flow_start_time[i] + TimeStep(1), trace_socket, 
                            blk_app.Get(0)->GetObject<BulkSendApplication>(), i);
    }

    // qlen trace
    qlen_f.open(qlen_file, std::ios::out);
    qlen_f << "Time(s)" << '\t' << "mx-pos" << '\t' << "qlen-max(p)"
           << std::endl;
    Simulator::Schedule(app_start_time, &PrintQlen);

    // pcap trace
    // PcapHelper pcapHelper;
    // Ptr<PcapFileWrapper> fileTx = 
    //     pcapHelper.CreateFile(name + "-261-tx.pcap", std::ios::out, 
    //                           PcapHelper::DLT_PPP);
    // Ptr<PcapFileWrapper> fileRx = 
    //     pcapHelper.CreateFile(name + "-261-rx.pcap", std::ios::out, 
    //                           PcapHelper::DLT_PPP);
    // nodes.Get(261)->GetObject<Ipv4>()->GetNetDevice(1)
    //     ->TraceConnectWithoutContext("PhyTxEnd", 
    //                                  MakeBoundCallback(&PcapPhy, 
    //                                                    fileTx));
    // nodes.Get(261)->GetObject<Ipv4>()->GetNetDevice(1)
    //     ->TraceConnectWithoutContext("PhyRxEnd", 
    //                                  MakeBoundCallback(&PcapPhy, 
    //                                                    fileRx));

    // start simulation
    auto start = high_resolution_clock::now();
    Simulator::Schedule(app_start_time, &printProgress);
    Simulator::Stop(stop_time + TimeStep(1));
    Simulator::Run();
    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<minutes>(stop - start);
    fprintf(stdout, "Simulation ends in %ld minutes\n", duration.count());

    FCT_f.open(FCT_file, std::ios::out);
    FCT_f << "flow size(B)" << '\t' << "FCT(us)" << std::endl;
    cal_FCT(flow_num);

    // close opened files
    topo_f.close();
    flow_f.close();
    FCT_f.close();
    // qlen_f.close();

    std::cout << "Total ACK: " << totalAck << std::endl;
    std::cout << "Total ACK with big-flow option: " << totalAckBigflow << std::endl;
    fprintf(stdout, "proportion: %.2lf%%\n", double(totalAckBigflow) / totalAck * 100);

    return 0;
}
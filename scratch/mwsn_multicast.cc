#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ClusterHeadMulticast");

const uint32_t nClusters = 3;
const uint32_t totalNodes = 10;
const double simulationTime = 10;
const Ipv4Address multicastGroup("225.1.2.3");
const uint16_t multicastPort = 8080;

void ReceiveAlert(Ptr<Socket> socket)
{
    Ptr<Packet> packet;
    Address from;
    
    while ((packet = socket->RecvFrom(from))) {
        if (packet->GetSize() > 0) {
            InetSocketAddress iaddr = InetSocketAddress::ConvertFrom(from);
            std::cout << Simulator::Now().GetSeconds() << "s: [ALERT RECEIVED] Node " 
                      << socket->GetNode()->GetId() << " received from " 
                      << iaddr.GetIpv4() << std::endl;
        }
    }
}

void SendAlert(Ptr<Socket> socket)
{
    Ptr<Packet> packet = Create<Packet>(5);
    socket->SendTo(packet, 0, InetSocketAddress(multicastGroup, multicastPort));
    std::cout << Simulator::Now().GetSeconds() << "s: [ALERT SENT] Node " 
              << socket->GetNode()->GetId() << std::endl;
}

int main(int argc, char *argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);
    
    LogComponentEnable("ClusterHeadMulticast", LOG_LEVEL_INFO);
    LogComponentEnable("UdpSocket", LOG_LEVEL_DEBUG);

    // Create nodes
    NodeContainer clusterHeads;
    clusterHeads.Create(nClusters);

    NodeContainer sensorNodes;
    sensorNodes.Create(totalNodes);

    // Configure WiFi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n);
    
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wifiPhy;
    wifiPhy.SetChannel(wifiChannel.Create());
    wifiPhy.Set("TxPowerStart", DoubleValue(20.0));
    wifiPhy.Set("TxPowerEnd", DoubleValue(20.0));

    WifiMacHelper wifiMac;
    Ssid ssid = Ssid("MWSN-Network");

    // Configure devices
    wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer chDevices = wifi.Install(wifiPhy, wifiMac, clusterHeads);

    wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer sensorDevices = wifi.Install(wifiPhy, wifiMac, sensorNodes);

    // Configure Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(30.0),
                                "DeltaY", DoubleValue(30.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(clusterHeads);
    
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
    mobility.Install(sensorNodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(clusterHeads);
    stack.Install(sensorNodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer chInterfaces = ipv4.Assign(chDevices);
    Ipv4InterfaceContainer sensorInterfaces = ipv4.Assign(sensorDevices);

    // Configure multicast routing
    Ipv4StaticRoutingHelper multicast;
    for (uint32_t i = 0; i < clusterHeads.GetN(); i++) {
        Ptr<Node> node = clusterHeads.Get(i);
        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
        ipv4->SetAttribute("IpForward", BooleanValue(true));
        
        Ptr<Ipv4StaticRouting> routing = multicast.GetStaticRouting(ipv4);
        uint32_t interface = ipv4->GetInterfaceForDevice(node->GetDevice(0));
        
        routing->AddMulticastRoute(
            multicastGroup,
            Ipv4Address::GetAny(),
            interface,
            std::vector<uint32_t>({interface})
        );
        
        routing->SetDefaultMulticastRoute(interface);
    }

    // Create and configure sockets - WORKING NS-3.43 VERSION
    for (uint32_t i = 0; i < clusterHeads.GetN(); i++) {
        Ptr<Node> node = clusterHeads.Get(i);
        
        // Receiver socket
        Ptr<Socket> recvSocket = Socket::CreateSocket(node, UdpSocketFactory::GetTypeId());
        
        // Bind to multicast group address
        if (recvSocket->Bind(InetSocketAddress(multicastGroup, multicastPort)) != 0) {
            NS_FATAL_ERROR("Failed to bind socket");
        }
        
        recvSocket->SetRecvCallback(MakeCallback(&ReceiveAlert));
        recvSocket->SetAllowBroadcast(true);
        
        // Sender socket
        Ptr<Socket> sendSocket = Socket::CreateSocket(node, UdpSocketFactory::GetTypeId());
        sendSocket->SetAllowBroadcast(true);
        
        // Schedule alerts
        Simulator::Schedule(Seconds(2.0 + i), &SendAlert, sendSocket);
    }

    // Enable packet capture
    wifiPhy.EnablePcapAll("multicast");

    // Animation configuration
    AnimationInterface anim("multicast.xml");
    anim.SetConstantPosition(clusterHeads.Get(0), 10.0, 10.0);
    anim.SetConstantPosition(clusterHeads.Get(1), 50.0, 10.0);
    anim.SetConstantPosition(clusterHeads.Get(2), 90.0, 10.0);

    // Run simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
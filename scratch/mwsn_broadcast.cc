#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ClusterHeadAlertSystem");

// Constants moved inside a namespace to avoid global scope pollution
namespace AlertSystemConstants {
    const uint32_t nClusters = 3;
    const uint32_t totalNodes = 10;
    const double simulationTime = 30.0;
    const Ipv4Address multicastGroup("224.1.2.3");
    const uint16_t alertPort = 8080;
    const uint16_t ackPort = 8081;
}

// Custom headers must be defined before they're used
class NodeIdHeader : public Header {
public:
    NodeIdHeader() : m_nodeId(0) {}
    NodeIdHeader(uint32_t nodeId) : m_nodeId(nodeId) {}
    
    static TypeId GetTypeId() {
        static TypeId tid = TypeId("ns3::NodeIdHeader")
            .SetParent<Header>()
            .AddConstructor<NodeIdHeader>();
        return tid;
    }
    
    uint32_t GetNodeId() const { return m_nodeId; }
    void SetNodeId(uint32_t nodeId) { m_nodeId = nodeId; }
    
    virtual TypeId GetInstanceTypeId() const { return GetTypeId(); }
    virtual void Print(std::ostream &os) const { os << "NodeId: " << m_nodeId; }
    virtual uint32_t GetSerializedSize() const { return sizeof(m_nodeId); }
    
    virtual void Serialize(Buffer::Iterator start) const {
        start.WriteHtonU32(m_nodeId);
    }
    
    virtual uint32_t Deserialize(Buffer::Iterator start) {
        m_nodeId = start.ReadNtohU32();
        return GetSerializedSize();
    }
    
private:
    uint32_t m_nodeId;
};

class AckHeader : public Header {
public:
    AckHeader() : m_responderId(0), m_senderId(0) {}
    
    static TypeId GetTypeId() {
        static TypeId tid = TypeId("ns3::AckHeader")
            .SetParent<Header>()
            .AddConstructor<AckHeader>();
        return tid;
    }
    
    uint32_t GetResponderId() const { return m_responderId; }
    uint32_t GetSenderId() const { return m_senderId; }
    void SetResponderId(uint32_t id) { m_responderId = id; }
    void SetSenderId(uint32_t id) { m_senderId = id; }
    
    virtual TypeId GetInstanceTypeId() const { return GetTypeId(); }
    virtual void Print(std::ostream &os) const { 
        os << "Ack from " << m_responderId << " to " << m_senderId; 
    }
    virtual uint32_t GetSerializedSize() const { return 2 * sizeof(uint32_t); }
    
    virtual void Serialize(Buffer::Iterator start) const {
        start.WriteHtonU32(m_responderId);
        start.WriteHtonU32(m_senderId);
    }
    
    virtual uint32_t Deserialize(Buffer::Iterator start) {
        m_responderId = start.ReadNtohU32();
        m_senderId = start.ReadNtohU32();
        return GetSerializedSize();
    }
    
private:
    uint32_t m_responderId;
    uint32_t m_senderId;
};

NS_OBJECT_ENSURE_REGISTERED(NodeIdHeader);
NS_OBJECT_ENSURE_REGISTERED(AckHeader);

class ClusterHeadAlertSystem {
public:
    ClusterHeadAlertSystem(NodeContainer &clusterHeads, NodeContainer &sensorNodes) 
        : m_clusterHeads(clusterHeads), m_sensorNodes(sensorNodes) {}

    void SetupNetwork() {
        using namespace AlertSystemConstants;
        
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
        m_chDevices = wifi.Install(wifiPhy, wifiMac, m_clusterHeads);

        wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
        m_sensorDevices = wifi.Install(wifiPhy, wifiMac, m_sensorNodes);

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
        mobility.Install(m_clusterHeads);
        
        mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                    "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                    "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
        mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
        mobility.Install(m_sensorNodes);

        // Install Internet stack
        InternetStackHelper stack;
        stack.Install(m_clusterHeads);
        stack.Install(m_sensorNodes);

        // Assign IP addresses
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        m_chInterfaces = ipv4.Assign(m_chDevices);
        m_sensorInterfaces = ipv4.Assign(m_sensorDevices);

        SetupMulticast();
        SetupSockets();
        ScheduleEvents();
    }

    void SetupMulticast() {
        using namespace AlertSystemConstants;
        
        Ipv4StaticRoutingHelper multicast;
        
        // Configure multicast routing for all cluster heads
        for (uint32_t i = 0; i < m_clusterHeads.GetN(); i++) {
            Ptr<Node> node = m_clusterHeads.Get(i);
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
    }
    
    void SetupSockets() {
        using namespace AlertSystemConstants;
        
        // Setup alert and ack sockets for all cluster heads
        for (uint32_t i = 0; i < m_clusterHeads.GetN(); i++) {
            Ptr<Node> node = m_clusterHeads.Get(i);
            uint32_t nodeId = node->GetId();
            
            // Alert receiver socket (multicast)
            Ptr<Socket> alertRecvSocket = Socket::CreateSocket(node, UdpSocketFactory::GetTypeId());
            alertRecvSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), alertPort));
            alertRecvSocket->SetRecvCallback(MakeCallback(&ClusterHeadAlertSystem::ReceiveAlert, this));
            
            // For WiFi multicast in NS-3.43, we just need to ensure proper configuration:
            // 1. Set the WifiPhy to maximum sensitivity
            Ptr<WifiNetDevice> wifiDev = DynamicCast<WifiNetDevice>(node->GetDevice(0));
            if (wifiDev) {
                Ptr<WifiPhy> wifiPhy = wifiDev->GetPhy();
                wifiPhy->SetRxSensitivity(-100); // Very sensitive receiver
                wifiPhy->SetTxPowerStart(20);    // High transmission power
                wifiPhy->SetTxPowerEnd(20);
            }
            
            // Ack receiver socket (unicast)
            Ptr<Socket> ackRecvSocket = Socket::CreateSocket(node, UdpSocketFactory::GetTypeId());
            ackRecvSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), ackPort));
            ackRecvSocket->SetRecvCallback(MakeCallback(&ClusterHeadAlertSystem::ReceiveAck, this));
            
            // Store sockets
            m_alertRecvSockets[nodeId] = alertRecvSocket;
            m_ackRecvSockets[nodeId] = ackRecvSocket;
            m_alertSendSockets[nodeId] = Socket::CreateSocket(node, UdpSocketFactory::GetTypeId());
            m_ackSendSockets[nodeId] = Socket::CreateSocket(node, UdpSocketFactory::GetTypeId());
            
            alertStatus[nodeId] = false;
        }
    }

    void ScheduleEvents() {
        using namespace AlertSystemConstants;
        
        // Random fault detection events for each cluster head
        for (uint32_t i = 0; i < m_clusterHeads.GetN(); i++) {
            Ptr<Node> node = m_clusterHeads.Get(i);
            double detectionTime = 5.0 + i * 5.0 + (rand() % 5); // Between 5-15 seconds
            
            Simulator::Schedule(Seconds(detectionTime), &ClusterHeadAlertSystem::DetectFault, this, node->GetId());
        }
        
        // Periodic status report
        Simulator::Schedule(Seconds(20.0), &ClusterHeadAlertSystem::ReportStatus, this);
    }

    void DetectFault(uint32_t nodeId) {
        using namespace AlertSystemConstants;
        
        NS_LOG_INFO(Simulator::Now().GetSeconds() << "s: Node " << nodeId << " detected a fault!");
        alertStatus[nodeId] = true;
        
        Ptr<Packet> packet = Create<Packet>(50);
        NodeIdHeader header(nodeId);
        packet->AddHeader(header);
        
        // Send to multicast group
        m_alertSendSockets[nodeId]->SendTo(packet, 0, 
                                         InetSocketAddress(multicastGroup, alertPort));
        
        NS_LOG_INFO(Simulator::Now().GetSeconds() << "s: Node " << nodeId 
                  << " sent ALERT to multicast group");
    }

    void ReceiveAlert(Ptr<Socket> socket) {
        Ptr<Packet> packet;
        Address from;
        
        while ((packet = socket->RecvFrom(from))) {
            NodeIdHeader header;
            packet->RemoveHeader(header);
            uint32_t senderId = header.GetNodeId();
            uint32_t receiverId = socket->GetNode()->GetId();
            
            if (senderId == receiverId) {
                continue; // Ignore our own messages
            }
            
            NS_LOG_INFO(Simulator::Now().GetSeconds() << "s: Node " << receiverId 
                      << " received ALERT from Node " << senderId);
            
            // Send acknowledgment back to sender
            SendAck(receiverId, senderId);
        }
    }

    void SendAck(uint32_t responderId, uint32_t senderId) {
        using namespace AlertSystemConstants;
        
        Ptr<Packet> packet = Create<Packet>(20);
        AckHeader header;
        header.SetResponderId(responderId);
        header.SetSenderId(senderId);
        packet->AddHeader(header);
        
        // Get sender's IP address
        Ipv4Address senderAddr = m_chInterfaces.GetAddress(senderId);
        
        // Send directly to the sender (unicast)
        m_ackSendSockets[responderId]->SendTo(packet, 0, 
                                            InetSocketAddress(senderAddr, ackPort));
        
        NS_LOG_INFO(Simulator::Now().GetSeconds() << "s: Node " << responderId 
                  << " sent ACK to Node " << senderId);
    }

    void ReceiveAck(Ptr<Socket> socket) {
        Ptr<Packet> packet;
        Address from;
        
        while ((packet = socket->RecvFrom(from))) {
            AckHeader header;
            packet->RemoveHeader(header);
            uint32_t responderId = header.GetResponderId();
            uint32_t senderId = header.GetSenderId();
            
            NS_LOG_INFO(Simulator::Now().GetSeconds() << "s: Node " << senderId 
                      << " received ACK from Node " << responderId);
            
            // Track which nodes have responded
            ackReceived[senderId].push_back(responderId);
        }
    }

    void ReportStatus() {
        NS_LOG_INFO("\n=== Final Alert Status Report ===");
        
        for (auto const& entry : alertStatus) {
            uint32_t nodeId = entry.first;
            bool detected = entry.second;
            
            NS_LOG_INFO("Node " << nodeId << ": " << (detected ? "DETECTED fault" : "No fault"));
            
            if (detected) {
                NS_LOG_INFO("  Received ACKs from: ");
                for (uint32_t responder : ackReceived[nodeId]) {
                    NS_LOG_INFO("    Node " << responder);
                }
            }
        }
    }

private:
    NodeContainer m_clusterHeads;
    NodeContainer m_sensorNodes;
    NetDeviceContainer m_chDevices;
    NetDeviceContainer m_sensorDevices;
    Ipv4InterfaceContainer m_chInterfaces;
    Ipv4InterfaceContainer m_sensorInterfaces;
    
    std::map<uint32_t, Ptr<Socket>> m_alertRecvSockets;
    std::map<uint32_t, Ptr<Socket>> m_ackRecvSockets;
    std::map<uint32_t, Ptr<Socket>> m_alertSendSockets;
    std::map<uint32_t, Ptr<Socket>> m_ackSendSockets;
    
    std::map<uint32_t, bool> alertStatus;
    std::map<uint32_t, std::vector<uint32_t>> ackReceived;
};

int main(int argc, char *argv[]) {
    using namespace AlertSystemConstants;
    
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);
    
    LogComponentEnable("ClusterHeadAlertSystem", LOG_LEVEL_INFO);
    LogComponentEnable("UdpSocket", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer clusterHeads;
    clusterHeads.Create(nClusters);

    NodeContainer sensorNodes;
    sensorNodes.Create(totalNodes - nClusters);

    // Setup the alert system
    ClusterHeadAlertSystem alertSystem(clusterHeads, sensorNodes);
    alertSystem.SetupNetwork();

    // Enable packet capture
    // YansWifiPhyHelper wifiPhy;
    // wifiPhy.EnablePcapAll("cluster_alert");

    // Animation configuration
    AnimationInterface anim("cluster_alert.xml");
    for (uint32_t i = 0; i < nClusters; i++) {
        anim.SetConstantPosition(clusterHeads.Get(i), 30.0 * i, 30.0);
    }

    // Run simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
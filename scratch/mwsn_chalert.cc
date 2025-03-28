#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"

#include <vector>
#include <stdio.h>  
#include <stdlib.h>  
#include <math.h>  
#include <ctype.h>  
#include <string.h>  
#include <time.h>  
#include <iterator>

#include <iostream>
#include <fstream>


#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#pragma GCC diagnostic ignored "-Wunused-variable" 
#pragma GCC diagnostic ignored "-Wunused-value" 
#pragma GCC diagnostic ignored "-Wwrite-strings" 
#pragma GCC diagnostic ignored "-Wparentheses" 


using namespace std;
using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ClusterHeadMulticast");

void ReceiveAlert(Ptr<Socket> socket)
{
    cout<<"ReceiveAlert"<< endl;
    Address from;
    Ptr<Packet> packet = socket->RecvFrom(from);
    std::cout << "[ALERT RECEIVED] Cluster Head " << socket->GetNode()->GetId()
              << " received an alert from " << InetSocketAddress::ConvertFrom(from).GetIpv4()
              << std::endl;
}

void SendAlert(Ptr<Socket> socket, Ipv4Address multicastGroup)
{
    Ptr<Packet> alertPacket = Create<Packet>((uint8_t *)"HELP!", 5);
    socket->SendTo(alertPacket, 0, InetSocketAddress(multicastGroup, 8080));
    std::cout << "[ALERT SENT] Cluster Head " << socket->GetNode()->GetId() << " sent an alert\n";
}

int main(int argc, char *argv[])
{
    uint32_t nClusters = 3; // Number of clusters (i.e., CHs)
    uint32_t nNodesPerCluster = 5;
    uint32_t totalNodes = nClusters * nNodesPerCluster;
    double simulationTime = 10.0;

    NodeContainer clusterHeads;
    clusterHeads.Create(nClusters);

    NodeContainer sensorNodes;
    sensorNodes.Create(totalNodes);

    // Install WiFi on both CHs and Sensors
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ax);
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wifiPhy;
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    Ssid ssid = Ssid("MWSN-Network");

    // Cluster Heads (AP Mode)
    wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer chDevices = wifi.Install(wifiPhy, wifiMac, clusterHeads);

    // Sensor Nodes (STA Mode)
    wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer sensorDevices = wifi.Install(wifiPhy, wifiMac, sensorNodes);

    // Mobility: CHs are stationary, sensors are randomly placed
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(clusterHeads);
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobility.Install(sensorNodes);

    // Install IP stack
    InternetStackHelper stack;
    stack.Install(clusterHeads);
    stack.Install(sensorNodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer chInterfaces = ipv4.Assign(chDevices);
    Ipv4InterfaceContainer sensorInterfaces = ipv4.Assign(sensorDevices);

    // Enable Multicast Routing
    Ipv4StaticRoutingHelper staticRouting;
    for (uint32_t i = 0; i < nClusters; i++)
    {
        Ptr<Node> chNode = clusterHeads.Get(i);
        Ptr<Ipv4> ipv4 = chNode->GetObject<Ipv4>();
        Ptr<Ipv4StaticRouting> staticRoute = staticRouting.GetStaticRouting(ipv4);

        // Ensure each CH joins the multicast group
        staticRoute->AddMulticastRoute(1, Ipv4Address("239.255.1.1"), Ipv4Address("10.1.1.255"), 1);
    }

    for (uint32_t i = 0; i < nClusters; i++)
    {
        Ptr<Node> chNode = clusterHeads.Get(i);
        Ptr<Ipv4> ipv4 = chNode->GetObject<Ipv4>();
        Ptr<Ipv4StaticRouting> staticRoute = staticRouting.GetStaticRouting(ipv4);
        staticRoute->SetDefaultMulticastRoute(1);
    }

    // Create Multicast Group for Cluster Heads
    Ipv4Address multicastGroup("239.255.1.1");

    // Create Sockets for Cluster Heads
    std::vector<Ptr<Socket>> chSockets;
    for (uint32_t i = 0; i < nClusters; i++)
    {
        Ptr<Socket> socket = Socket::CreateSocket(clusterHeads.Get(i), UdpSocketFactory::GetTypeId());
        socket->Bind(InetSocketAddress(Ipv4Address::GetAny(), 8080));
        socket->SetRecvCallback(MakeCallback(&ReceiveAlert));
        chSockets.push_back(socket);
    }

    // Schedule Alerts (CH0 will send an alert at t=2s)
    Simulator::Schedule(Seconds(2.0), &SendAlert, chSockets[0], multicastGroup);

    // NetAnim Visualization
    AnimationInterface anim("mwsn-clusterhead-multicast.xml");
    anim.EnablePacketMetadata(true);
    anim.UpdateNodeColor(clusterHeads.Get(0), 255, 0, 0); // CH0 Red
    anim.UpdateNodeColor(clusterHeads.Get(1), 0, 255, 0); // CH1 Green
    anim.UpdateNodeColor(clusterHeads.Get(2), 0, 0, 255); // CH2 Blue

    // Run Simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

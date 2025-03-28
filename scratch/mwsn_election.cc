#include <iostream>
#include <vector>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <iterator>
#include <fstream>
#include "include/delaunator.hpp"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/log.h"

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-value"
#pragma GCC diagnostic ignored "-Wwrite-strings"
#pragma GCC diagnostic ignored "-Wparentheses"

using namespace ns3;
using namespace std;

// Constants
#define MIN_ENERGY_THRESHOLD 5.0
#define MIN_FAULT_TOLERANCE_PERCENTAGE 10
#define SENSING_RANGE 25.0

int NUM_NODES = 30;
int NETWORK_X = 100;
int NETWORK_Y = 100;

// Simulation parameters
double B_POWER = 0.5;
double CLUSTER_PERCENT = 0.3;  
const int TOTAL_ROUNDS = 40000;  
double LEACH_AD_DISTANCE = 25;    
int LEACH_AD_MESSAGE = 16;    
double SCHEDULE_DISTANCE = 25;    
int SCHEDULE_MESSAGE = 16;  
double Rs = 25;
int BASE_STATION_X_DEFAULT = 300;
int BASE_STATION_Y_DEFAULT = 300;  
int DEAD_NODE = -2;
int MESSAGE_LENGTH = 8;
int TRIALS = 1;

// Utility functions
double computeCircumradius(const pair<double, double>& A, 
                         const pair<double, double>& B, 
                         const pair<double, double>& C) {
    double a = hypot(B.first - C.first, B.second - C.second);
    double b = hypot(A.first - C.first, A.second - C.second);
    double c = hypot(A.first - B.first, A.second - B.second);
    double s = (a + b + c) / 2.0;
    double area = sqrt(s * (s - a) * (s - b) * (s - c));
    return (a * b * c) / (4.0 * area);
}

bool isObtuseTriangle(double a, double b, double c) {
    return (a * a + b * b < c * c) || (b * b + c * c < a * a) || (c * c + a * a < b * b);
}

double distance(double x1, double y1, double x2, double y2) {
    return hypot(x1 - x2, y1 - y2);
}

bool isFullyCovered(double x1, double y1, double x2, double y2, double x3, double y3, double rs) {
    double side[3];
    side[0] = distance(x1, y1, x2, y2);
    side[1] = distance(x3, y3, x2, y2);
    side[2] = distance(x1, y1, x3, y3);

    int maxid = 0;
    if (side[1] > side[maxid]) maxid = 1;
    if (side[2] > side[maxid]) maxid = 2;

    double angle[2];
    angle[0] = acos((side[maxid] * side[maxid] + side[(maxid + 1) % 3] * side[(maxid + 1) % 3] - side[(maxid + 2) % 3] * side[(maxid + 2) % 3]) / (2 * side[maxid] * side[(maxid + 1) % 3]));
    angle[1] = acos((side[maxid] * side[maxid] + side[(maxid + 2) % 3] * side[(maxid + 2) % 3] - side[(maxid + 1) % 3] * side[(maxid + 1) % 3]) / (2 * side[maxid] * side[(maxid + 2) % 3]));

    double pbl[2];
    pbl[0] = tan(angle[0]) * (side[(maxid + 1) % 3] / 2);
    pbl[1] = tan(angle[1]) * (side[(maxid + 2) % 3] / 2);

    double length[2];
    length[0] = sqrt(pbl[0] * pbl[0] + (side[(maxid + 1) % 3] / 2) * (side[(maxid + 1) % 3] / 2));
    length[1] = sqrt(pbl[1] * pbl[1] + (side[(maxid + 2) % 3] / 2) * (side[(maxid + 2) % 3] / 2));

    return !(length[0] > rs || length[1] > rs);
}

string CHANGE = "Mod_CLUSTER_PERCENT0_3";

struct clusterHead;

struct sensor {
    int id = -1;
    int clusterId = -1;
    short xLoc = 0;
    short yLoc = 0;
    short lPeriods = 0;
    double bCurrent = 0.5;
    double bPower = 0.5;
    double pAverage = 0.0;
    bool isFaulty = false;
    clusterHead* head = nullptr;
    double distance_BASE = 0.0;
    double distance_current_head = 0.0;
    double V_bPower[TOTAL_ROUNDS] = {0};
    int V_cluster_members[TOTAL_ROUNDS] = {0};

    sensor() = default;
    
    void updateFaultStatus();
};

struct clusterHead : public sensor {
    vector<sensor*> members;
    vector<sensor*> faulty;
    vector<vector<double>> hole_table;
    int faultNodeCount = 0;
    double totalNodes = 0;
    double faultPercentage = 0.0;
    vector<double> coords;

    clusterHead() = default;
    
    void updateFaultNodeCount(sensor* faultNode) {
        if (!faultNode) return;
        
        faulty.push_back(faultNode);
        faultNodeCount++;  
        faultPercentage = (faultNodeCount / totalNodes) * 100.0;
        if (faultNodeCount > 2) {
            cout << "Hole: " << checkForHole();
        }
    }

    bool checkForHole() {
        if (members.empty()) return false;
        
        if (coords.empty()) {
            for (auto &node : members) {
                if (node) {
                    coords.push_back(node->xLoc);
                    coords.push_back(node->yLoc);
                }
            }
        }
        
        try {
            delaunator::Delaunator d(coords);
            vector<vector<double>> triangles;
            
            for (std::size_t i = 0; i < d.triangles.size(); i += 3) {
                vector<double> triangle;
                triangle.push_back(d.coords[2 * d.triangles[i]]);
                triangle.push_back(d.coords[2 * d.triangles[i] + 1]);
                triangle.push_back(d.coords[2 * d.triangles[i + 1]]);
                triangle.push_back(d.coords[2 * d.triangles[i + 1] + 1]);
                triangle.push_back(d.coords[2 * d.triangles[i + 2]]);
                triangle.push_back(d.coords[2 * d.triangles[i + 2] + 1]);
                triangles.push_back(triangle);
            }
            
            for (auto& triangle : triangles) {
                if (triangle.size() < 6) continue;
                
                pair<double, double> A = {triangle[0], triangle[1]};
                pair<double, double> B = {triangle[2], triangle[3]};
                pair<double, double> C = {triangle[4], triangle[5]};
                
                double Rc = computeCircumradius(A, B, C);
                if (Rc > Rs) {
                    double a = distance(B.first, B.second, C.first, C.second);
                    double b = distance(A.first, A.second, C.first, C.second);
                    double c = distance(A.first, A.second, B.first, B.second);

                    if (isObtuseTriangle(a, b, c)) {
                        if (!isFullyCovered(A.first, A.second, B.first, B.second, C.first, C.second, Rs)) {
                            hole_table.push_back(triangle);
                        }
                    } else {
                        hole_table.push_back(triangle);
                    }
                }
            }
        } catch (const std::exception& e) {
            cerr << "Delaunay triangulation error: " << e.what() << endl;
            return false;
        }
        
        return !hole_table.empty();
    }
};

void sensor::updateFaultStatus() {
    if (bCurrent <= MIN_ENERGY_THRESHOLD && !isFaulty) {
        isFaulty = true;
        if (head) {
            head->updateFaultNodeCount(this);
        }
        cout << "Node " << id << " declared as FAULTY in Cluster " << clusterId << endl;
    }
}

struct sensor BASE_STATION;

struct network_stats {
    int BASE_STATION_X = 0;
    int BASE_STATION_Y = 0;
    int NETWORK_X = 0;
    int NETWORK_Y = 0;
    int NUM_NODES = 0;
    double network_comparison = 0.0;
    int LEACH_ROUNDS = 0;
    int LEACH_NEW_ROUNDS = 0;
    int DIRECT_ROUNDS = 0;
    double Improvement = 0.0;
    double CLUSTER_PERCENT = 0.0;
    int LEACH_threshold = 0;
    int LEACH_NEW_rounds[TOTAL_ROUNDS] = {0};
    double LEACH_NEW_network_average_energy[TOTAL_ROUNDS] = {0};
    double LEACH_NEW_network_total_energy[TOTAL_ROUNDS] = {0};
    int LEACH_NEW_num_dead_node[TOTAL_ROUNDS] = {0};
    int LEACH_NEW_num_cluster_head[TOTAL_ROUNDS] = {0};
    double LEACH_NEW_percent_head[TOTAL_ROUNDS] = {0};
};

static sensor* FindOptimalReplacement(vector<clusterHead*>& allClusters, sensor* failedNode) {
    if (!failedNode) return nullptr;
    
    sensor* bestCandidate = nullptr;
    double maxScore = -1.0;

    for (auto cluster : allClusters) {
        if (!cluster) continue;

        for (auto candidate : cluster->members) {
            if (!candidate) continue;

            cout<<"Checking candidate: " << candidate->id 
                        << " | Faulty: " << candidate->isFaulty 
                        << " | Same as failed node: " << (candidate == failedNode) 
                        << " | Is CH: " << (candidate == static_cast<sensor*>(cluster))<<endl;

            if (candidate->isFaulty || candidate == failedNode || 
                candidate == static_cast<sensor*>(cluster))
                continue;

            double energyScore = candidate->bCurrent / candidate->bPower;
            double dist = distance(candidate->xLoc, candidate->yLoc, 
                                 failedNode->xLoc, failedNode->yLoc);
            double distanceScore = 1.0 / (1.0 + dist);
            
            int coverageCount = 0;
            for (auto node : cluster->members) {
                if (node && distance(candidate->xLoc, candidate->yLoc, 
                                  node->xLoc, node->yLoc) <= SENSING_RANGE) {
                    coverageCount++;
                }
            }
            double coverageScore = cluster->members.empty() ? 0 : coverageCount / (double)cluster->members.size();

            cout<<"Candidate " << candidate->id << " has energy score: " << energyScore
                        << " | distance score: " << distanceScore
                        << " | coverage score: " << coverageScore<<endl;

            double score = 0.4 * energyScore + 0.3 * distanceScore + 0.3 * coverageScore;
            
            if (score > maxScore) {
                cout<<"New best candidate found: " << candidate->id << " with score: " << score<<endl;
                maxScore = score;
                bestCandidate = candidate;
            }
        }
    }

    if (!bestCandidate) {
        cout<<"No suitable replacement node found!"<<endl;
    }
    
    return bestCandidate;
}


void initializeNetwork(sensor sensors[], clusterHead CHs[]) {  
    // Initialize cluster heads
    CHs[0].id = 1;
    CHs[0].clusterId = 1;
    CHs[0].xLoc = 50;
    CHs[0].yLoc = 50;
    
    CHs[1].id = 2;
    CHs[1].clusterId = 2;
    CHs[1].xLoc = 30;
    CHs[1].yLoc = 70;
    
    CHs[2].id = 3;
    CHs[2].clusterId = 3;
    CHs[2].xLoc = 70;
    CHs[2].yLoc = 70;
    
    CHs[3].id = 4;
    CHs[3].clusterId = 4;
    CHs[3].xLoc = 70;
    CHs[3].yLoc = 30;
    
    CHs[4].id = 5;
    CHs[4].clusterId = 5;
    CHs[4].xLoc = 30;
    CHs[4].yLoc = 30;
    
    // Initialize sensors
    int id = 6;
    int s_ctr = 0;
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 5; j++) {
            sensors[s_ctr].id = id;
            sensors[s_ctr].clusterId = i + 1;
            sensors[s_ctr].xLoc = CHs[i].xLoc - 10 + (rand() % 20);
            sensors[s_ctr].yLoc = CHs[i].yLoc - 10 + (rand() % 20);
            sensors[s_ctr].head = &CHs[i];
            CHs[i].members.push_back(&sensors[s_ctr]);
            CHs[i].totalNodes++;
            
            s_ctr++;
            id++;
        }
    }
}

int main(int argc, char *argv[]) {
    // Initialize NS-3 logging first
    cout<<"Starting simulation..."<<endl;

    try {
        // 1. Command line parsing
        cout<<"Parsing command line arguments"<<endl;
        CommandLine cmd(__FILE__);
        cmd.Parse(argc, argv);

        // 2. Replace arrays with vectors to avoid stack overflow
        cout<<"Creating network nodes"<<endl;
        std::vector<sensor> sensors(25);  // Using vector instead of array
        std::vector<clusterHead> CHs(5);  // Using vector instead of array

        // 3. Initialize network
        cout<<"Initializing network topology"<<endl;
        initializeNetwork(sensors.data(), CHs.data());  // Using .data() to get raw pointers

        // 4. NS-3 node setup
        cout<<"Creating NS-3 nodes"<<endl;
        NodeContainer nodes;
        nodes.Create(NUM_NODES);  // Make sure NUM_NODES matches your total nodes (25 sensors + 5 CHs = 30)

        // 5. Mobility setup
        cout<<"Setting up mobility"<<endl;
        MobilityHelper mobility;
        Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
        
        // Add sensor positions
        for (size_t i = 0; i < sensors.size(); ++i) {
            positionAlloc->Add(Vector(sensors[i].xLoc, sensors[i].yLoc, 0.0));
        }
        
        // Add cluster head positions
        for (size_t i = 0; i < CHs.size(); ++i) {
            positionAlloc->Add(Vector(CHs[i].xLoc, CHs[i].yLoc, 0.0));
        }
        
        mobility.SetPositionAllocator(positionAlloc);
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(nodes);

        // 6. Animation setup with bounds checking
        cout<<"Setting up animation"<<endl;
        AnimationInterface anim("mwsn.xml");
        
        // Color mapping
        vector<tuple<int, uint8_t, uint8_t, uint8_t>> clusterColors = {
            {1, 0, 255, 0},   // Green
            {2, 0, 0, 255},   // Blue
            {3, 255, 255, 0}, // Yellow
            {4, 255, 165, 0}, // Orange
            {5, 128, 0, 128}   // Purple
        };

        // Set positions and colors for sensors
        for (size_t i = 0; i < sensors.size(); ++i) {
            if (i >= nodes.GetN()) {
                cout<<"Sensor index out of bounds: " << i<<endl;
                continue;
            }
            anim.SetConstantPosition(nodes.Get(i), sensors[i].xLoc, sensors[i].yLoc);
            
            for (const auto& [clusterId, r, g, b] : clusterColors) {
                if (sensors[i].clusterId == clusterId) {
                    anim.UpdateNodeColor(nodes.Get(i), r, g, b);
                    break;
                }
            }
        }

        // Set positions and colors for cluster heads (red)
        for (size_t i = 0; i < CHs.size(); ++i) {
            size_t nodeIndex = sensors.size() + i;
            if (nodeIndex >= nodes.GetN()) {
                cout<<"CH index out of bounds: " << nodeIndex<<endl;
                continue;
            }
            anim.SetConstantPosition(nodes.Get(nodeIndex), CHs[i].xLoc, CHs[i].yLoc);
            anim.UpdateNodeColor(nodes.Get(nodeIndex), 255, 0, 0);
        }

        // 7. Simulate node failure with validation
        cout<<"Simulating node failure"<<endl;
        if (sensors.size() > 7) {
            sensor* failedNode = &sensors[7];
            failedNode->bCurrent = 0;
            failedNode->updateFaultStatus();

            // Check for coverage holes
            if (failedNode->head && failedNode->head->checkForHole()) {
                cout<<"Coverage hole detected in cluster " << failedNode->clusterId<<endl;
                
                // Create a vector containing all cluster heads
                vector<clusterHead*> allClusters;
                for (auto& ch : CHs) {
                    allClusters.push_back(&ch);
                }

                sensor* replacement = FindOptimalReplacement(allClusters, failedNode);
                    
                if (replacement) {
                    cout<<"Selected replacement node " << replacement->id<<endl;
                    size_t replacementIndex = replacement->id - 6; // Adjust based on your ID scheme
                    if (replacementIndex < nodes.GetN()) {
                        anim.UpdateNodeColor(nodes.Get(replacementIndex), 0, 0, 255);
                        anim.UpdateNodeDescription(nodes.Get(failedNode->id - 6), "Failed");
                        anim.UpdateNodeDescription(nodes.Get(replacementIndex), "Replacement");
                    }
                }
            }
        } else {
            cout<<"Not enough sensors for failure simulation"<<endl;
        }

        // 8. Run simulation
        cout<<"Starting simulation"<<endl;
        Simulator::Run();
        Simulator::Destroy();
        cout<<"Simulation completed"<<endl;

    } catch (const std::exception& e) {
        cout<<"Exception: " << e.what()<<endl;
        return -1;
    }

    return 0;
}
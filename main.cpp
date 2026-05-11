#define GVDLL
#define NOMINMAX
#include <lcf/lmu/reader.h>
#include <lcf/ldb/reader.h>
#include <lcf/lmt/reader.h>
#include <lcf/rpg/map.h>
#include <lcf/rpg/database.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <unordered_set>
#include <windows.h>
#include <sstream>
#include <iomanip>
#include <memory>
#include <thread>
#include <mutex>
#include <chrono>
#include <atomic>
#include <queue>

#include <cgraph.h>
#include <gvc.h>

enum class Condition {
    None,
    TargetMoving,
    EffectApplied,
    Unknown
};

class Node;

struct Edge {
    int  destIndex;
    Condition cond;
    int  weight;

    Edge(int destIndex, Condition cond = Condition::None, int weight = 1)
        : destIndex(destIndex), cond(cond), weight(weight) {}
};

class Node {
    std::vector<Edge> edges;
public:
    std::string name;

    Node() = default;
    explicit Node(const std::string& name) : name(name) {}

    void connectTo(int destIndex, Condition cond = Condition::None, int weight = 1) {
        edges.emplace_back(destIndex, cond, weight);
    }

    const std::vector<Edge>& getEdges() const { return edges; }
};

class Graph {
    std::vector<Node> nodes;
    mutable std::mutex mtx;
public:
    int addNode(Node node) {
        std::lock_guard<std::mutex> lock(mtx);
        nodes.push_back(std::move(node));
        return static_cast<int>(nodes.size()) - 1;
    }

    void connect(int from, int to,
             Condition cond = Condition::None, int weight = 1) {
        std::lock_guard<std::mutex> lock(mtx);
        nodes[from].connectTo(to, cond, weight);
    }

    void draw() {
        std::lock_guard<std::mutex> lock(mtx);
        if (nodes.empty()) return;

        GVC_t* gvc = gvContext();
        Agraph_t* g = agopen((char*)"mi_grafo", Agdirected, nullptr);

        agattr(g, AGNODE, (char*)"shape",     (char*)"circle");
        agattr(g, AGNODE, (char*)"style",     (char*)"filled");
        agattr(g, AGNODE, (char*)"fillcolor", (char*)"lightblue");
        agattr(g, AGNODE, (char*)"fontname",  (char*)"Helvetica");
        agattr(g, AGEDGE, (char*)"color",     (char*)"gray40");
        agattr(g, AGEDGE, (char*)"fontname",  (char*)"Helvetica");
        agattr(g, AGEDGE, (char*)"fontsize",  (char*)"10");

        std::vector<Agnode_t*> agNodes(nodes.size());
        for (int i = 0; i < (int)nodes.size(); ++i) {
            agNodes[i] = agnode(g, (char*)nodes[i].name.c_str(), 1);
        }

        for (int i = 0; i < (int)nodes.size(); ++i) {
            for (const Edge& edge : nodes[i].getEdges()) {
                Agnode_t* src  = agNodes[i];
                Agnode_t* dest = agNodes[edge.destIndex];  // ← siempre válido
                Agedge_t* e = agedge(g, src, dest, nullptr, 1);
                std::string label = "peso: " + std::to_string(edge.weight);
                agsafeset(e, (char*)"label", (char*)label.c_str(), (char*)"");
            }
        }

        gvLayout(gvc, g, "sfdp");
        //gvRenderFilename(gvc, g, "png", "grafo.png");
        //gvRenderFilename(gvc, g, "svg", "grafo.svg");
        gvRenderFilename(gvc, g, "dot", "grafo.dot");

        gvFreeLayout(gvc, g);
        agclose(g);
        gvFreeContext(gvc);
    }
};

// Converts UTF-16 wide string to UTF-8
std::string wideToUtf8(const std::wstring& wide) {
    if (wide.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring basePath;
std::unique_ptr<lcf::rpg::Database> db = nullptr;
std::unique_ptr<lcf::rpg::TreeMap> treeMap = nullptr;
std::unique_ptr<lcf::rpg::Map> map = nullptr;

lcf::rpg::MapInfo* getMapInfo(int mapId) {
    if (treeMap == nullptr) {
        throw std::runtime_error("Error: treeMap no cargado.");
    }

    lcf::rpg::MapInfo* it = (&treeMap->maps[0] + mapId);

    if (it != nullptr) {
        std::cout << "Nombre del mapa: " << std::string(it->name) << std::endl;
        return it;
    } else {
        std::cerr << "Error: No se encontró el mapa con ID " << mapId << " en el TreeMap." << std::endl;
        return nullptr;
    }
}

std::unique_ptr<lcf::rpg::Map> loadMap(const std::wstring& basePath, int mapId) {
    std::wstringstream ss;
    ss << basePath << L"/Map" << std::setw(4) << std::setfill(L'0') << mapId << L".lmu";
    std::wstring mapPathW = ss.str();

    std::ifstream mapStream(mapPathW, std::ios::binary);
    if (!mapStream.is_open()) {
        std::cerr << "Error: No se pudo abrir el archivo del mapa con ID " << mapId << "." << std::endl;
        return nullptr;
    }

    return lcf::LMU_Reader::Load(mapStream, "cp932");
}

void findConnectingMaps(std::unique_ptr<Graph>& graph, int startMapId) {
    std::unordered_set<int> visited;
    std::queue<int> cola;  // <-- la clave del cambio

    cola.push(startMapId);
    visited.insert(startMapId);

    while (!cola.empty()) {
        int mapId = cola.front();
        cola.pop();

        auto map = loadMap(basePath, mapId);
        if (!map) {
            std::cerr << "Error al cargar el mapa ID: " << mapId << std::endl;
            continue;
        }

        std::cout << "Mapa cargado con éxito! ID: " << mapId << std::endl;
        std::cout << "Dimensiones: " << map->width << "x" << map->height << std::endl;

        if (treeMap == nullptr) {
            throw std::runtime_error("Error: treeMap no cargado.");
        }

        lcf::rpg::MapInfo* info = getMapInfo(mapId);
        std::cout << "Nombre del mapa: " << std::string(info->name) << std::endl;
        int thisNodeId = graph->addNode(Node("Teletransporte a " + std::string(info->name)));

        /*
        // Find the chipset used by this map
        const lcf::rpg::Chipset* chipset = nullptr;
        if (db) {
            for (const auto& c : db->chipsets) {
                if (c.ID == map->chipset_id) {
                    chipset = &c;
                    break;
                }
            }
        }

        if (chipset) {
            std::cout << "Chipset encontrado: " << std::string(chipset->name) << std::endl;

            // Example: Scan the first 10x10 tiles for collision
            int limitX = std::min(100, map->width);
            int limitY = std::min(100, map->height);

            for (int y = 0; y < limitY; ++y) {
                for (int x = 0; x < limitX; ++x) {
                    int tileIndex = x + y * map->width;

                    // Check logic
                    bool blocked = false;

                    // 1. Check Upper Layer (Events/Overlays)
                    // Note: This is simplified. Real RPG Maker logic is complex.
                    // Upper tiles usually explicitly block if they are not "Star" (Overhead).
                    int upperId = map->upper_layer[tileIndex];
                    if (upperId >= 10000) {
                        int uIdx = upperId - 10000;
                        if (uIdx < chipset->passable_data_upper.size()) {
                            uint8_t flags = chipset->passable_data_upper[uIdx];
                            // 0x10 is "Star" (Over Hero). If NOT star, it might block.
                            // If flags == 0, it is fully blocked.
                            // If flags == 15 (0x0F), it is fully passable.
                            // We treat "Not Star" and "Not Passable" as blocked for simplicity.
                            if (!(flags & 0x10)) { // Not Star
                                // Check directional passability (simplified: if 0, block all)
                                if ((flags & 0x0F) != 0x0F) blocked = true;
                            }
                        }
                    }

                    // 2. Check Lower Layer (Terrain) if not already blocked
                    // Lower layer logic:
                    // IDs 0-2999 are Autotiles. Map by dividing by 50 (approx).
                    // IDs 3000+ are Terrain. Map by subtracting 3000 and adding offset (usually 18).
                    if (!blocked) {
                        int lowerId = map->lower_layer[tileIndex];
                        int lIdx = -1;

                        if (lowerId >= 3000) {
                            lIdx = 18 + (lowerId - 3000);
                        } else {
                            lIdx = lowerId / 50;
                        }

                        if (lIdx >= 0 && lIdx < static_cast<int>(chipset->passable_data_lower.size())) {
                            uint8_t flags = chipset->passable_data_lower[lIdx];
                            // Check directional passability
                            if ((flags & 0x0F) != 0x0F) blocked = true;
                        }
                    }

                    // Output result
                    std::cout << (blocked ? "X" : ".");
                }
                std::cout << std::endl;
            }
        }
        */

        for (const auto& event : map->events) {
            for (const auto& page : event.pages) {
                for (const auto& cmd : page.event_commands) {
                    if (cmd.code == 10810) {
                        int destId = cmd.parameters[0];
                        std::cout << "Found tp command to world: " << destId << std::endl;

                        auto it = getMapInfo(destId);

                        if (it != nullptr) {
                            std::cout << "  Nombre del destino: " << std::string(it->name) << std::endl;
                            int newNodeId = graph->addNode(Node(std::string(it->name)));
                            graph->connect(thisNodeId, newNodeId, Condition::TargetMoving);
                        }

                        // En lugar de llamada recursiva, encola si no fue visitado
                        if (!visited.count(destId)) {
                            visited.insert(destId);
                            cola.push(destId);
                        }
                    }
                }
            }
        }
    }
}

int wmain(int argc, wchar_t* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    if (argc < 2) {
        std::cerr << "Uso: Yume-2kki-Graph.exe <ruta_al_juego>" << std::endl;
        return 1;
    }

    basePath = argv[1];

    // NEW: Load the Database to get collision data
    std::wstring ldbPathW = basePath + L"/RPG_RT.ldb";
    std::ifstream ldbStream(ldbPathW, std::ios::binary);

    if (ldbStream.is_open()) {
        db = lcf::LDB_Reader::Load(ldbStream, "cp932");
    } else {
        std::cerr << "Advertencia: No se pudo cargar RPG_RT.ldb. La detección de colisiones no funcionará." << std::endl;
    }

    // Load the Map Tree to get map names
    std::wstring lmtPathW = basePath + L"/RPG_RT.lmt";
    std::ifstream lmtStream(lmtPathW, std::ios::binary);

    if (lmtStream.is_open()) {
        treeMap = lcf::LMT_Reader::Load(lmtStream, "cp932");
    } else {
        std::cerr << "Advertencia: No se pudo cargar RPG_RT.lmt. Los nombres de mapa no estarán disponibles." << std::endl;
    }

    std::unique_ptr<Graph> graph = std::make_unique<Graph>();
    std::atomic<bool> isGenerating{true};

    std::thread drawThread([&]() {
        while (isGenerating) {
            std::this_thread::sleep_for(std::chrono::seconds(10));
            if (isGenerating) {
                graph->draw();
            }
        }
    });

    findConnectingMaps(graph, 10);

    isGenerating = false;
    drawThread.join();

    graph->draw();

    return 0;
}

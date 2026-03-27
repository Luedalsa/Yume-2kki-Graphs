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
#include <windows.h>
#include <sstream>
#include <iomanip>
#include <memory>

#include <cgraph.h>
#include <gvc.h>

enum class Condition {
    None,
    TargetMoving,
    EffectApplied,
    Unknown
};

struct Edge {
    int destination;
    Condition requiredCondition;
    int weight;
};

class Node {
    std::vector<Edge> edges;
};

// Converts UTF-16 wide string to UTF-8
std::string wideToUtf8(const std::wstring& wide) {
    if (wide.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, result.data(), size, nullptr, nullptr);
    return result;
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

void createGraphSVG() {
    // ── 1. Contexto de Graphviz ─────────────────────────────────────────────
    GVC_t *gvc = gvContext();

    // ── 2. Crear el grafo dirigido ──────────────────────────────────────────
    // Agdirected  → grafo dirigido (digraph)
    // Agundirected → grafo no dirigido
    Agraph_t *g = agopen((char*)"mi_grafo", Agdirected, nullptr);

    // ── 3. Atributos globales (se aplican a todos los nodos/aristas) ────────
    agattr(g, AGNODE, (char*)"shape",     "circle");
    agattr(g, AGNODE, (char*)"style",     "filled");
    agattr(g, AGNODE, (char*)"fillcolor", "lightblue");
    agattr(g, AGNODE, (char*)"fontname",  "Helvetica");

    agattr(g, AGEDGE, (char*)"color",    "gray40");
    agattr(g, AGEDGE, (char*)"fontname", "Helvetica");
    agattr(g, AGEDGE, (char*)"fontsize", "10");

    // ── 4. Crear nodos ──────────────────────────────────────────────────────
    // agnode(grafo, nombre_interno, crear_si_no_existe)
    Agnode_t *nA = agnode(g, (char*)"A", 1);
    Agnode_t *nB = agnode(g, (char*)"B", 1);
    Agnode_t *nC = agnode(g, (char*)"C", 1);
    Agnode_t *nD = agnode(g, (char*)"D", 1);

    // Personalizar nodos individualmente
    agsafeset(nA, (char*)"label",     (char*)"Inicio",    (char*)"");
    agsafeset(nA, (char*)"fillcolor", (char*)"#4CAF50",   (char*)"");
    agsafeset(nA, (char*)"fontcolor", (char*)"white",     (char*)"");

    agsafeset(nD, (char*)"label",     (char*)"Fin",       (char*)"");
    agsafeset(nD, (char*)"fillcolor", (char*)"#F44336",   (char*)"");
    agsafeset(nD, (char*)"fontcolor", (char*)"white",     (char*)"");
    agsafeset(nD, (char*)"shape",     (char*)"doublecircle", (char*)"");

    // ── 5. Crear aristas ────────────────────────────────────────────────────
    // agedge(grafo, nodo_origen, nodo_destino, nombre, crear_si_no_existe)
    Agedge_t *eAB = agedge(g, nA, nB, (char*)"AB", 1);
    Agedge_t *eAC = agedge(g, nA, nC, (char*)"AC", 1);
    Agedge_t *eBD = agedge(g, nB, nD, (char*)"BD", 1);
    Agedge_t *eCD = agedge(g, nC, nD, (char*)"CD", 1);

    // Etiquetar aristas
    agsafeset(eAB, (char*)"label", (char*)"peso: 3", (char*)"");
    agsafeset(eAC, (char*)"label", (char*)"peso: 1", (char*)"");
    agsafeset(eBD, (char*)"label", (char*)"peso: 5", (char*)"");
    agsafeset(eCD, (char*)"label", (char*)"peso: 2", (char*)"");

    // Marcar el camino más corto (A→C→D) con color diferente
    agsafeset(eAC, (char*)"color",     (char*)"#FF6600", (char*)"");
    agsafeset(eAC, (char*)"penwidth",  (char*)"2.5",     (char*)"");
    agsafeset(eCD, (char*)"color",     (char*)"#FF6600", (char*)"");
    agsafeset(eCD, (char*)"penwidth",  (char*)"2.5",     (char*)"");

    // ── 6. Aplicar layout y renderizar ─────────────────────────────────────
    // Algoritmos disponibles: "dot" (jerárquico), "neato", "fdp", "circo", "twopi"
    gvLayout(gvc, g, "dot");

    gvRenderFilename(gvc, g, "png", "grafo.png");   // Imagen PNG
    gvRenderFilename(gvc, g, "svg", "grafo.svg");   // SVG (vectorial)
    gvRenderFilename(gvc, g, "dot", "grafo.dot");   // Código DOT generado

    printf("Archivos generados:\n");
    printf("  grafo.png  — imagen\n");
    printf("  grafo.svg  — vectorial\n");
    printf("  grafo.dot  — codigo DOT\n");

    // ── 7. Liberar memoria ──────────────────────────────────────────────────
    gvFreeLayout(gvc, g);
    agclose(g);
    gvFreeContext(gvc);
}

int wmain(int argc, wchar_t* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    if (argc < 2) {
        std::cerr << "Uso: Yume-2kki-Graph.exe <ruta_al_juego>" << std::endl;
        return 1;
    }

    std::wstring basePath(argv[1]);

    // Use helper function to load map
    auto map = loadMap(basePath, 10);

    if (!map) {
        return 1;
    }

    // NEW: Load the Database to get collision data
    std::wstring ldbPathW = basePath + L"/RPG_RT.ldb";
    std::ifstream ldbStream(ldbPathW, std::ios::binary);
    std::unique_ptr<lcf::rpg::Database> db = nullptr;

    if (ldbStream.is_open()) {
        db = lcf::LDB_Reader::Load(ldbStream, "cp932");
    } else {
        std::cerr << "Advertencia: No se pudo cargar RPG_RT.ldb. La detección de colisiones no funcionará." << std::endl;
    }

    // Load the Map Tree to get map names
    std::wstring lmtPathW = basePath + L"/RPG_RT.lmt";
    std::ifstream lmtStream(lmtPathW, std::ios::binary);
    std::unique_ptr<lcf::rpg::TreeMap> treeMap = nullptr;

    if (lmtStream.is_open()) {
        treeMap = lcf::LMT_Reader::Load(lmtStream, "cp932");
    } else {
        std::cerr << "Advertencia: No se pudo cargar RPG_RT.lmt. Los nombres de mapa no estarán disponibles." << std::endl;
    }

    if (map) {
        std::cout << "Mapa cargado con éxito!" << std::endl;
        std::cout << "Dimensiones: " << map->width << "x" << map->height << std::endl;

        if (treeMap) {
            auto it = std::find_if(treeMap->maps.begin(), treeMap->maps.end(), [&](const lcf::rpg::MapInfo& info) {
                return info.ID == 10;
            });
            if (it != treeMap->maps.end()) {
                std::cout << "Nombre del mapa: " << std::string(it->name) << std::endl;
            }
        }

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

        for (const auto& event : map->events) {
            //std::cout << "Evento detectado: " << event.name << std::endl;
            for (const auto& pages : event.pages) {
                //std::cout << "  Página con condiciones: " << pages.condition << " y " << pages.event_commands.size() << " comandos" << std::endl;
                for (const auto& cmd : pages.event_commands) {
                    //std::cout << "    Comando: " << cmd.code << " - " << cmd.parameters.size() << " parámetros" << std::endl;
                    if (cmd.code == 10810) {
                        std::cout << "Found tp command to world: " << cmd.parameters[0] << std::endl;

                        if (treeMap) {
                            auto it = std::find_if(treeMap->maps.begin(), treeMap->maps.end(), [&](const lcf::rpg::MapInfo& info) {
                                return info.ID == cmd.parameters[0];
                            });
                            if (it != treeMap->maps.end()) {
                                std::cout << "  Nombre del destino: " << std::string(it->name) << std::endl;
                            }
                        }

                        auto targetMap = loadMap(basePath, cmd.parameters[0]);
                        if (targetMap) {
                            std::cout << "Mapa de destino cargado con éxito!" << std::endl;
                            std::cout << "Dimensiones: " << targetMap->width << "x" << targetMap->height << std::endl;
                        } else {
                            std::cerr << "Error al cargar el mapa de destino." << std::endl;
                        }
                    }
                }
            }
        }
    } else {
        std::cerr << "Error al cargar el mapa." << std::endl;
    }

    createGraphSVG();

    return 0;
}
#define NOMINMAX
#include <lcf/lmu/reader.h>
#include <lcf/ldb/reader.h>
#include <lcf/rpg/map.h>
#include <lcf/rpg/database.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <windows.h>

// Converts UTF-16 wide string to UTF-8
std::string wideToUtf8(const std::wstring& wide) {
    if (wide.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, result.data(), size, nullptr, nullptr);
    return result;
}

int wmain(int argc, wchar_t* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    if (argc < 2) {
        std::cerr << "Uso: Yume-2kki-Graph.exe <ruta_al_juego>" << std::endl;
        return 1;
    }

    // Build the wide path directly — no encoding corruption
    std::wstring mapPathW = std::wstring(argv[1]) + L"/Map0002.lmu";

    // Open the file ourselves using the wide path (Windows handles Japanese correctly)
    std::ifstream mapStream(mapPathW, std::ios::binary);
    if (!mapStream.is_open()) {
        std::cerr << "Error: No se pudo abrir el archivo del mapa." << std::endl;
        return 1;
    }

    // Pass the stream to lcf — bypasses its internal (broken) file open
    auto map = lcf::LMU_Reader::Load(mapStream, "cp932");

    // NEW: Load the Database to get collision data
    std::wstring ldbPathW = std::wstring(argv[1]) + L"/RPG_RT.ldb";
    std::ifstream ldbStream(ldbPathW, std::ios::binary);
    std::unique_ptr<lcf::rpg::Database> db = nullptr;

    if (ldbStream.is_open()) {
        db = lcf::LDB_Reader::Load(ldbStream, "cp932");
    } else {
        std::cerr << "Advertencia: No se pudo cargar RPG_RT.ldb. La detección de colisiones no funcionará." << std::endl;
    }

    if (map) {
        std::cout << "Mapa cargado con éxito!" << std::endl;
        std::cout << "Dimensiones: " << map->width << "x" << map->height << std::endl;

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
            std::cout << "Evento detectado: " << event.name << std::endl;
        }
    } else {
        std::cerr << "Error al cargar el mapa." << std::endl;
    }

    return 0;
}
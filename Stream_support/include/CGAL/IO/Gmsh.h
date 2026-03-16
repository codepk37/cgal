// Minimal Gmsh (.msh v2) ASCII reader for polygon-soup import
// Adds a simple parser that extracts points and triangular elements.
// This is an initial, lightweight implementation for integration/testing.
// It intentionally focuses on nodes and triangle elements (elmType == 2).

#ifndef CGAL_IO_GMSH_H
#define CGAL_IO_GMSH_H

#include <string>
#include <vector>
#include <istream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <array>
#include <algorithm>

#include <CGAL/Polygon_mesh_processing/orient_polygon_soup.h>

namespace CGAL {
namespace IO {

// Read a Gmsh v2 ASCII stream into a point list and polygon index list.
// Point must be constructible from three doubles: Point(x,y,z)
// Polygons are returned as vectors of vertex indices (0-based into points).
template <typename Point, typename Index = std::size_t>
bool read_MSH(std::istream& is,
              std::vector<Point>& points,
              std::vector<std::vector<Index>>& polygons,
              std::vector<std::vector<Index>>& cells)
{
  points.clear();
  polygons.clear();
  cells.clear();

  std::string line;
  std::unordered_map<long long, std::size_t> id_to_idx;

  while (std::getline(is, line)) {
    if (line.empty()) continue;
    if (line[0] == '$') {
      if (line == "$Nodes") {
        // Next line: number of nodes
        if (!std::getline(is, line)) return false;
        std::istringstream iss(line);
        std::size_t n_nodes = 0;
        iss >> n_nodes;

        for (std::size_t i = 0; i < n_nodes; ++i) {
          if (!std::getline(is, line)) return false;
          if (line.empty()) { --i; continue; }
          std::istringstream ns(line);
          long long id; double x, y, z;
          ns >> id >> x >> y >> z;
          std::size_t idx = points.size();
          id_to_idx[id] = idx;
          points.emplace_back(x, y, z);
        }

        // consume $EndNodes
        std::getline(is, line);
        continue;
      }
      else if (line == "$Elements") {
        // Next: number of elements
        if (!std::getline(is, line)) return false;
        std::istringstream iss(line);
        std::size_t n_elms = 0;
        iss >> n_elms;

        for (std::size_t i = 0; i < n_elms; ++i) {
          if (!std::getline(is, line)) return false;
          if (line.empty()) { --i; continue; }
          std::istringstream es(line);
          long long id; int elmType; int numTags;
          es >> id >> elmType >> numTags;
          // read tags
          for (int t = 0; t < numTags; ++t) { long long tmp; es >> tmp; }

          // Read remaining integers on the line as node ids
          std::vector<long long> node_ids;
          long long nid;
          while (es >> nid) node_ids.push_back(nid);

          // Convert to indices
          std::vector<Index> idxs;
          idxs.reserve(node_ids.size());
          for (auto nidv : node_ids) {
            auto it = id_to_idx.find(nidv);
            if (it == id_to_idx.end()) return false; // unknown node
            idxs.push_back(static_cast<Index>(it->second));
          }

          // Classify by element type
          switch (elmType) {
            case 2: // triangle
            case 3: // quadrangle
              polygons.push_back(std::move(idxs));
              break;
            case 4: // tetrahedron
            case 5: // hexahedron
            case 6: // prism
            case 7: // pyramid
              cells.push_back(std::move(idxs));
              break;
            default:
              // fallback: if clearly 2D (3 or 4 nodes), treat as polygon; otherwise as cell
              if (idxs.size() == 3 || idxs.size() == 4)
                polygons.push_back(std::move(idxs));
              else
                cells.push_back(std::move(idxs));
              break;
          }
        }

        // consume $EndElements
        std::getline(is, line);
        continue;
      }
      else {
        // Unknown $section, skip until matching $End...
        std::string endtag = std::string("$End") + line.substr(1);
        while (std::getline(is, line)) {
          if (line == endtag) break;
        }
        continue;
      }
    }
    // ignore non-section lines
  }

  // Extract boundary triangles from volume cells if no surface polygons were provided.
  // This lets tetrahedral meshes visualize their outer hull as a triangle soup.
  if(polygons.empty() && !cells.empty()) {
    struct FaceData {
      int count = 0;
      std::array<Index,3> oriented{{0,0,0}}; // first-seen orientation
    };
    struct ArrayHash {
      std::size_t operator()(const std::array<Index,3>& a) const noexcept {
        return std::hash<Index>()(a[0]) ^ (std::hash<Index>()(a[1]) << 1) ^ (std::hash<Index>()(a[2]) << 2);
      }
    };

    std::unordered_map<std::array<Index,3>, FaceData, ArrayHash> faces;

    auto add_face = [&](Index a, Index b, Index c){
      std::array<Index,3> key{{a,b,c}};
      std::sort(key.begin(), key.end()); // orientation-agnostic key
      FaceData& fd = faces[key];
      ++fd.count;
      if(fd.count == 1) {
        fd.oriented = {a,b,c}; // store orientation from the generating cell
      }
    };

    for(const auto& cell : cells) {
      if(cell.size() == 4) {
        // Tetrahedron faces (orientation as given by cell ordering)
        add_face(cell[0], cell[1], cell[2]);
        add_face(cell[0], cell[1], cell[3]);
        add_face(cell[0], cell[2], cell[3]);
        add_face(cell[1], cell[2], cell[3]);
      }
      // Additional volume element types can be added here (hex/prism), but are skipped for now.
    }

    // Keep boundary faces (those appearing exactly once) with stored orientation
    for(const auto& kv : faces) {
      if(kv.second.count == 1) {
        const auto& f = kv.second.oriented;
        polygons.emplace_back(f.begin(), f.end());
      }
    }
  }

  // Ensure consistent orientation of the polygon soup (needed for Surface_mesh construction)
  CGAL::Polygon_mesh_processing::orient_polygon_soup(points, polygons);

  return true;
}

// Filename overload
template <typename Point, typename Index = std::size_t>
bool read_MSH(const std::string& fname,
              std::vector<Point>& points,
              std::vector<std::vector<Index>>& polygons,
              std::vector<std::vector<Index>>& cells)
{
  std::ifstream is(fname);
  if (!is) return false;
  return read_MSH<Point, Index>(is, points, polygons, cells);
}

// Filename overload
template <typename Point, typename Index = std::size_t>
bool read_MSH(const std::string& fname,
              std::vector<Point>& points,
              std::vector<std::vector<Index>>& polygons)
{
  std::ifstream is(fname);
  if (!is) return false;
  return read_MSH<Point, Index>(is, points, polygons);
}

} // namespace IO
} // namespace CGAL

#endif // CGAL_IO_GMSH_H

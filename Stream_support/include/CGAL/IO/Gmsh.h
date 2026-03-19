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
#include <limits>

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

  auto nodes_per_element = [](int type) -> int {
    switch(type)
    {
      case 1: return 2;   // line
      case 2: return 3;   // triangle
      case 3: return 4;   // quadrangle
      case 4: return 4;   // tetrahedron
      case 5: return 8;   // hexahedron
      case 6: return 6;   // prism
      case 7: return 5;   // pyramid
      case 8: return 3;   // second order line
      case 9: return 6;   // second order triangle
      case 10: return 9;  // second order quadrangle
      case 11: return 10; // second order tetrahedron
      case 12: return 27; // second order hexahedron
      case 13: return 18; // second order prism
      case 14: return 14; // second order pyramid
      case 15: return 1;  // point
      case 16: return 8;  // serendipity quadrangle
      case 17: return 20; // serendipity hexahedron
      case 18: return 15; // serendipity prism
      case 19: return 13; // serendipity pyramid
      case 20: return 9;  // third order triangle (6n/9n variants)
      case 21: return 10; // third order triangle (10 nodes)
      case 22: return 12; // third order quadrangle
      case 23: return 15; // fourth order triangle
      case 24: return 15; // fourth order quadrangle
      case 25: return 21; // fifth order triangle
      case 26: return 4;  // edge with 4 nodes
      case 27: return 5;  // edge with 5 nodes
      case 28: return 6;  // edge with 6 nodes
      case 29: return 20; // third order tetrahedron
      case 30: return 35; // fourth order tetrahedron
      case 31: return 56; // fifth order tetrahedron
      case 92: return 64; // third order hexahedron
      case 93: return 125; // fourth order hexahedron
      default: return -1;
    }
  };
  // https://gmsh.info/doc/texinfo/gmsh.html#MSH-file-format

  auto classify_element = [&](int elmType, std::vector<Index>&& idxs)
  {
    switch(elmType)
    {
      case 2:  // triangle
      case 3:  // quad
      case 9:  // second order triangle
      case 10: // second order quad
      case 16: // serendipity quad
      case 20: // higher order triangle (fallback as polygon)
      case 21:
      case 22: // higher order quad
      case 23:
      case 24: // higher order quad
      case 25: // higher order triangle
        polygons.push_back(std::move(idxs));
        return;
      case 4:  // tetra
      case 5:  // hex
      case 6:  // prism
      case 7:  // pyramid
      case 11: // second order tetra
      case 12: // second order hex
      case 13: // second order prism
      case 14: // second order pyramid
      case 17: // serendipity hex
      case 18: // serendipity prism
      case 19: // serendipity pyramid
      case 29: // higher order tetrahedra
      case 30:
      case 31:
      case 92: // higher order hexahedra
      case 93:
        cells.push_back(std::move(idxs));
        return;
      default:
        if(idxs.size() == 3 || idxs.size() == 4)
          polygons.push_back(std::move(idxs));
        else if(!idxs.empty())
          cells.push_back(std::move(idxs));
        break;
    }
  };

  auto skip_to_next_line = [&](std::istream& input) {
    input.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
  };

  auto read_next_non_empty_line = [&](std::string& out) -> bool {
    while(std::getline(is, out))
    {
      if(!out.empty())
        return true;
    }
    return false;
  };

  bool binary_mode = false;

  while(std::getline(is, line))
  {
    if(line.empty())
      continue;
    if(line[0] != '$')
      continue;

    if(line == "$MeshFormat")
    {
      if(!read_next_non_empty_line(line))
        return false;
      std::istringstream header(line);
      double version = 0.0;
      int binary_flag = 0;
      int data_size = 0;
      header >> version >> binary_flag >> data_size;
      if(!header)
        return false;

      if(binary_flag == 1)
      {
        binary_mode = true;
        int one = 0;
        is.read(reinterpret_cast<char*>(&one), sizeof(int));
        if(!is || one != 1)
          return false; // unsupported endianness or corrupted file
        skip_to_next_line(is);
      }

      if(!read_next_non_empty_line(line) || line != "$EndMeshFormat")
        return false;
      continue;
    }
    else if(line == "$Nodes")
    {
      if(!read_next_non_empty_line(line))
        return false;
      std::istringstream iss(line);
      std::size_t n_nodes = 0;
      iss >> n_nodes;
      if(!iss)
        return false;

      if(binary_mode)
      {
        for(std::size_t i = 0; i < n_nodes; ++i)
        {
          int node_id = 0;
          double coords[3];
          is.read(reinterpret_cast<char*>(&node_id), sizeof(int));
          is.read(reinterpret_cast<char*>(coords), 3 * sizeof(double));
          if(!is)
            return false;

          const std::size_t idx = points.size();
          id_to_idx[node_id] = idx;
          points.emplace_back(coords[0], coords[1], coords[2]);
        }
        skip_to_next_line(is);
      }
      else
      {
        for(std::size_t i = 0; i < n_nodes; ++i)
        {
          if(!std::getline(is, line))
            return false;
          if(line.empty())
          {
            --i;
            continue;
          }
          std::istringstream ns(line);
          long long id;
          double x, y, z;
          ns >> id >> x >> y >> z;
          if(!ns)
            return false;
          const std::size_t idx = points.size();
          id_to_idx[id] = idx;
          points.emplace_back(x, y, z);
        }
      }

      if(!read_next_non_empty_line(line) || line != "$EndNodes")
        return false;
      continue;
    }
    else if(line == "$Elements")
    {
      if(!read_next_non_empty_line(line))
        return false;
      std::istringstream iss(line);
      std::size_t n_elms = 0;
      iss >> n_elms;
      if(!iss)
        return false;

      if(binary_mode)
      {
        std::size_t read_elements = 0;
        while(read_elements < n_elms)
        {
          int elmType = 0;
          int block_count = 0;
          int numTags = 0;
          is.read(reinterpret_cast<char*>(&elmType), sizeof(int));
          is.read(reinterpret_cast<char*>(&block_count), sizeof(int));
          is.read(reinterpret_cast<char*>(&numTags), sizeof(int));
          if(!is)
            return false;

          const int node_count = nodes_per_element(elmType);
          if(node_count < 0)
            return false; // unsupported element type in binary mode

          std::vector<int> tags(numTags, 0);
          std::vector<int> node_ids(node_count, 0);
          for(int j = 0; j < block_count; ++j)
          {
            int elem_id = 0;
            is.read(reinterpret_cast<char*>(&elem_id), sizeof(int));
            if(numTags > 0)
            {
              is.read(reinterpret_cast<char*>(tags.data()), numTags * sizeof(int));
            }
            if(node_count > 0)
            {
              is.read(reinterpret_cast<char*>(node_ids.data()), node_count * sizeof(int));
            }
            if(!is)
              return false;

            std::vector<Index> idxs;
            idxs.reserve(node_ids.size());
            for(int nid : node_ids)
            {
              auto it = id_to_idx.find(nid);
              if(it == id_to_idx.end())
                return false;
              idxs.push_back(static_cast<Index>(it->second));
            }

            classify_element(elmType, std::move(idxs));
          }

          read_elements += static_cast<std::size_t>(block_count);
        }
        skip_to_next_line(is);
      }
      else
      {
        for(std::size_t i = 0; i < n_elms; ++i)
        {
          if(!std::getline(is, line))
            return false;
          if(line.empty())
          {
            --i;
            continue;
          }
          std::istringstream es(line);
          long long id;
          int elmType;
          int numTags;
          es >> id >> elmType >> numTags;
          if(!es)
            return false;
          for(int t = 0; t < numTags; ++t)
          {
            long long tmp;
            es >> tmp;
          }

          std::vector<long long> node_ids;
          long long nid;
          while(es >> nid)
            node_ids.push_back(nid);

          std::vector<Index> idxs;
          idxs.reserve(node_ids.size());
          for(auto nidv : node_ids)
          {
            auto it = id_to_idx.find(nidv);
            if(it == id_to_idx.end())
              return false;
            idxs.push_back(static_cast<Index>(it->second));
          }

          classify_element(elmType, std::move(idxs));
        }
      }

      if(!read_next_non_empty_line(line) || line != "$EndElements")
        return false;
      continue;
    }
    else
    {
      // Unknown $section, skip until matching $End...
      const std::string endtag = std::string("$End") + line.substr(1);
      while(std::getline(is, line))
      {
        if(line == endtag)
          break;
      }
      continue;
    }
  }

  if(polygons.empty() && !cells.empty())
  {
    struct FaceData
    {
      int count = 0;
      std::array<Index, 3> oriented{{0, 0, 0}};
    };
    struct ArrayHash
    {
      std::size_t operator()(const std::array<Index,3>& a) const noexcept
      {
        return std::hash<Index>()(a[0]) ^ (std::hash<Index>()(a[1]) << 1) ^ (std::hash<Index>()(a[2]) << 2);
      }
    };

    std::unordered_map<std::array<Index,3>, FaceData, ArrayHash> faces;

    auto add_face = [&](Index a, Index b, Index c)
    {
      std::array<Index,3> key{{a,b,c}};
      std::sort(key.begin(), key.end());
      FaceData& fd = faces[key];
      ++fd.count;
      if(fd.count == 1)
        fd.oriented = {a,b,c};
    };

    for(const auto& cell : cells)
    {
      if(cell.size() == 4)
      {
        add_face(cell[0], cell[1], cell[2]);
        add_face(cell[0], cell[1], cell[3]);
        add_face(cell[0], cell[2], cell[3]);
        add_face(cell[1], cell[2], cell[3]);
      }
    }

    for(const auto& kv : faces)
    {
      if(kv.second.count == 1)
      {
        const auto& f = kv.second.oriented;
        polygons.emplace_back(f.begin(), f.end());
      }
    }
  }

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
  std::ifstream is(fname, std::ios::binary);
  if (!is) return false;
  return read_MSH<Point, Index>(is, points, polygons, cells);
}

// Filename overload
template <typename Point, typename Index = std::size_t>
bool read_MSH(const std::string& fname,
              std::vector<Point>& points,
              std::vector<std::vector<Index>>& polygons)
{
  std::ifstream is(fname, std::ios::binary);
  if (!is) return false;
  return read_MSH<Point, Index>(is, points, polygons);
}

} // namespace IO
} // namespace CGAL

#endif // CGAL_IO_GMSH_H

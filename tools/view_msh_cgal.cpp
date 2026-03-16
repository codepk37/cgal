// Minimal CGAL-based viewer for .msh (and other) polygon meshes using CGAL::draw.

#include <CGAL/Simple_cartesian.h>
#include <CGAL/Surface_mesh.h>
#include <CGAL/Polygon_mesh_processing/IO/polygon_mesh_io.h>
#include <CGAL/draw_surface_mesh.h>

using Kernel = CGAL::Simple_cartesian<double>;
using Point = Kernel::Point_3;
using Mesh = CGAL::Surface_mesh<Point>;

int main(int argc, char* argv[]) {
  if(argc < 2) {
    std::cerr << "Usage: " << argv[0] << " mesh.msh" << std::endl;
    return 1;
  }

  Mesh mesh;
  if(!CGAL::IO::read_polygon_mesh(argv[1], mesh, CGAL::parameters::verbose(true))) {
    std::cerr << "Failed to load mesh: " << argv[1] << std::endl;
    return 1;
  }

  std::cout << "Vertices: " << num_vertices(mesh)
            << " Faces: " << num_faces(mesh) << std::endl;

  CGAL::draw(mesh);
  return 0;
}

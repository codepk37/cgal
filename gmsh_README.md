cd /home/pavan/Desktop/cgal/build && cmake --build . -j$(nproc)

cd /home/pavan/msh_viewer/build && cmake .. -DCMAKE_BUILD_TYPE=Release -DCGAL_DIR=/home/pavan/Desktop/cgal/build

make -j$(nproc)

./view_msh_cgal /home/pavan/Desktop/cube_mesh.msh

cmake --build . -j$(nproc)

./view_msh_cgal /home/pavan/Desktop/cube_mesh.msh


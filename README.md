# toothlib
## install
First clone the reposity, then run next command:
```
mkdir build && cd build
cmake .. -DUSER_ID=<您的USER_ID> -DUSER_TOKEN=<您的ZH_TOKEN> -DSERVER_URL=<BASE_URL> -DFILE_SERVER_URL=<FILE_SERVER_URL>
make
```
## run
**当前在编译之前，需要修改生成的libigl代码：**
`toothlib\build\_deps\libigl-src\include\igl\opengl\glfw\Viewer.cpp`下，修改`IGL_INLINE void Viewer::open_dialog_load_mesh()`为：
```
  IGL_INLINE std::string Viewer::open_dialog_load_mesh()
  {
    std::string fname = igl::file_dialog_open();

    if (fname.length() == 0)
      return 0;

    this->load_mesh_from_file(fname.c_str());
    
    return fname;
  }
```
且对应头文件`toothlib\build\_deps\libigl-src\include\igl\opengl\glfw\Viewer.h`下，修改`IGL_INLINE void Viewer::open_dialog_load_mesh()`为``IGL_INLINE std::string Viewer::open_dialog_load_mesh()``

之后可正常编译运行。

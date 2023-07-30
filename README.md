# toothlib
## install
首先clone项目到本地，之后在项目根目录下运行以下指令：（cmake使用vs 16 2019 generator）

```
mkdir build && cd build
cmake .. -DUSER_ID=<您的USER_ID> -DUSER_TOKEN=<您的ZH_TOKEN> -DSERVER_URL=<BASE_URL> -DFILE_SERVER_URL=<FILE_SERVER_URL>
```
## run
生成sln后将toothlib设为启动项，之后可正常生成、编译、运行。

## for libtorch
1. 下载libtorch：https://download.pytorch.org/libtorch/lts/1.8/cpu/libtorch-win-shared-with-deps-debug-1.8.2%2Bcpu.zip
2. 解压后将libtorch放到/cmake下
3. 编译时可能有一组错误："optional" ambiguous symbol，所有报错的符号前面加"std::"
4. 运行时显示缺失c10.dll等 将/cmake/libtorch/lib下的dll拷贝到/build/Debug下
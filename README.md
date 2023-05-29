# toothlib
## install
首先clone项目到本地，之后在项目根目录下运行以下指令：（cmake使用vs 16 2019 generator）

```
mkdir build && cd build
cmake .. -DUSER_ID=<您的USER_ID> -DUSER_TOKEN=<您的ZH_TOKEN> -DSERVER_URL=<BASE_URL> -DFILE_SERVER_URL=<FILE_SERVER_URL>
```
## run
生成sln后将toothlib设为启动项，之后可正常生成、编译、运行。

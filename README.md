C++音视频流媒体服务器SRS源码框架解读，配置文件(SrsConfig)的使用.

SRS配置文件的使用

SRS封装了SrsConfig类，定义了使用方便且功能强大的配置文件，部分参考了nginx的配置文件，形式上类似与JSON，一个配置项里面可以包含子配置项，套娃的形式和JSON数组很像；可重新加载配置，重载配置时通过回调函数的形式执行其他模块的重载。

功能概述：
1、通过命令行传参，解析main()函数命令行参数（parse_options），定义了传参选项：-h（打印帮助），-t（测试配置文件），-e（使用环境变量配置），-v（打印版本号），-g（打印签名），-c（读配置文件）。

2、SRS支持从系统环境变量读取配置信息，不使用配置文件（env_only_），包括配置文件名也可以从环境变量读取（srs_getenv(“srs.config.file”)）。

3、SRS默认策略是：如果系统环境变量有配置，优先使用系统环境变量的配置，其次使用配置文件的配置，最后使用默认值。

4、SRS封装了配置指令类（SrsConfDirective），一个配置指令包括：指令所在配置文件的行号（conf_line），指令名称，指令值数组（args，多个参数），子指令数组（directives，一个指令可包含多个子指令，数据结构一样）。

5、定义了根指令对象（root = new SrsConfDirective），先把配置文件读入缓存区（SrsConfigBuffer），再从缓存区解析配置，存入根指令（parse_conf）。

6、SRS提供了检测配置是否合法的接口（check_config），检测内容包括：各项配置字段是否合法，配置指令值是否合法（很细致的检测，配置文件一个标点符号都不能错）；检测配置的最大连接数是否小于等于系统支持的最大连接数（sysconf(_SC_OPEN_MAX)）。

7、提供了重载配置文件的接口（reload），可通过linux信号量实现配置文件重新加载。

8、当重载配置文件时，可执行各模块的回调函数，实现重载模块功能，模块类继承于ISrsReloadHandler，使用_srs_config->subscribe(this)注册重载，unsubscribe可取消注册重载，注册重载的模块加入数组（subscribes），重载时遍历执行数组（subscribes）里的回调函数。

9、配置重载流程：收到信号SRS_SIGNAL_RELOAD，_srs_config->reload()，重新解析配置文件parse_file，检测配置check_config，重载配置reload_conf，遍历执行subscribes注册的回调。

10、配置文件包括大量vhost内容，本章不讨论。


编译运行：

1、把配置文件srs.conf拷贝到构建目录。

2、加入命令行参数：-c srs.conf。

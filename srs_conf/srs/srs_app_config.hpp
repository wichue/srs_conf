#ifndef SRS_APP_CONFIG_HPP
#define SRS_APP_CONFIG_HPP


#include <vector>
#include <string>
#include <map>
#include <sstream>
#include <algorithm>
#include "chw_adapt.h"
#include "srs_kernel_error.hpp"
#include <srs_app_reload.hpp>
//#include <srs_app_async_call.hpp>
//#include <srs_app_st.hpp>

class SrsRequest;
class SrsFileWriter;
class SrsJsonObject;
class SrsJsonArray;
class SrsJsonAny;

class SrsConfig;
class SrsRequest;
class SrsJsonArray;
class SrsConfDirective;

/**
 * whether the two vector actual equals, for instance,
 *      srs_vector_actual_equals([0, 1, 2], [0, 1, 2])      ==== true
 *      srs_vector_actual_equals([0, 1, 2], [2, 1, 0])      ==== true
 *      srs_vector_actual_equals([0, 1, 2], [0, 2, 1])      ==== true
 *      srs_vector_actual_equals([0, 1, 2], [0, 1, 2, 3])   ==== false
 *      srs_vector_actual_equals([1, 2, 3], [0, 1, 2])      ==== false
 */
 // 两个vector数组是否完全相等，即a的元素都在b里面，b的元素也都在a里面
template<typename T>
bool srs_vector_actual_equals(const std::vector<T>& a, const std::vector<T>& b)
{
    // all elements of a in b.
    for (int i = 0; i < (int)a.size(); i++) {
        const T& e = a.at(i);
        if (std::find(b.begin(), b.end(), e) == b.end()) {
            return false;
        }
    }

    // all elements of b in a.
    for (int i = 0; i < (int)b.size(); i++) {
        const T& e = b.at(i);
        if (std::find(a.begin(), a.end(), e) == a.end()) {
            return false;
        }
    }

    return true;
}

namespace srs_internal
{
    // The buffer of config content.
    // 用于保存配置内容的缓冲区
    class SrsConfigBuffer
    {
    protected:
        // The last available position.最后一个可用位置
        char* last;
        // The end of buffer.缓冲区的末尾。
        char* end;
        // The start of buffer.缓冲区的开始。
        char* start;
    public:
        // Current consumed position.当前消耗的位置。
        char* pos;
        // Current parsed line.当前解析的行。
        int line;
    public:
        SrsConfigBuffer();
        virtual ~SrsConfigBuffer();
    public:
        // Fullfill the buffer with content of file specified by filename.
        // 用文件名所指定的文件的内容来填充缓冲区。
        virtual srs_error_t fullfill(const char* filename);
        // Whether buffer is empty.
        // 缓冲区是否为空
        virtual bool empty();
    };
};

// Deep compare directive.  深度比较两个配置指令是否相等
extern bool srs_directive_equals(SrsConfDirective* a, SrsConfDirective* b);
extern bool srs_directive_equals(SrsConfDirective* a, SrsConfDirective* b, std::string except);

// The helper utilities, used for compare the consts values.
// 辅助工具，用于比较常量字符串
extern bool srs_config_hls_is_on_error_ignore(std::string strategy);
extern bool srs_config_hls_is_on_error_continue(std::string strategy);
extern bool srs_config_ingest_is_file(std::string type);
extern bool srs_config_ingest_is_stream(std::string type);
extern bool srs_config_dvr_is_plan_segment(std::string plan);
extern bool srs_config_dvr_is_plan_session(std::string plan);
extern bool srs_stream_caster_is_udp(std::string caster);
extern bool srs_stream_caster_is_flv(std::string caster);
extern bool srs_stream_caster_is_gb28181(std::string caster);


// Convert bool in str to on/off
// 传入true返回on，传入false返回off
extern std::string srs_config_bool2switch(std::string sbool);

// Parse loaded vhost directives to compatible mode.
// For exmaple, SRS1/2 use the follow refer style:
//          refer   a.domain.com b.domain.com;
// while SRS3 use the following:
//          refer {
//              enabled on;
//              all a.domain.com b.domain.com;
//          }
// so we must transform the vhost directive anytime load the config.
// @param root the root directive to transform, in and out parameter.
// 将加载的vhost指令解析到兼容模式。
//extern srs_error_t srs_config_transform_vhost(SrsConfDirective* root);

// @global config object. 配置全局对象
extern SrsConfig* _srs_config;

// The config directive.
// The config file is a group of directives,
// all directive has name, args and child-directives.
// For example, the following config text:
//      vhost vhost.ossrs.net {
//       enabled         on;
//       ingest livestream {
//      	 enabled      on;
//      	 ffmpeg       /bin/ffmpeg;
//       }
//      }
// will be parsed to:
//      SrsConfDirective: name="vhost", arg0="vhost.ossrs.net", child-directives=[
//          SrsConfDirective: name="enabled", arg0="on", child-directives=[]
//          SrsConfDirective: name="ingest", arg0="livestream", child-directives=[
//              SrsConfDirective: name="enabled", arg0="on", child-directives=[]
//              SrsConfDirective: name="ffmpeg", arg0="/bin/ffmpeg", child-directives=[]
//          ]
//      ]
// @remark, allow empty directive, for example: "dir0 {}"
// @remark, don't allow empty name, for example: ";" or "{dir0 arg0;}
//配置指令类。配置文件是一组指令，所有的指令都有名称、参数和子指令。 套娃的形式个JSON数组很像。
class SrsConfDirective
{
public:
    // The line of config file in which the directive from
    // 指令来自配置文件的哪一行
    int conf_line;
    // The name of directive, for example, the following config text:
    //       enabled     on;
    // will be parsed to a directive, its name is "enalbed"
    std::string name;//指令名称
    // The args of directive, for example, the following config text:
    //       listen      1935 1936;
    // will be parsed to a directive, its args is ["1935", "1936"].
    std::vector<std::string> args;//指令数组，可以存放多个参数值
    // The child directives, for example, the following config text:
    //       vhost vhost.ossrs.net {
    //           enabled         on;
    //       }
    // will be parsed to a directive, its directives is a vector contains
    // a directive, which is:
    //       name:"enalbed", args:["on"], directives:[]
    //
    // @remark, the directives can contains directives.
    std::vector<SrsConfDirective*> directives;//子指令数组，指令里可以包含一个或多个子指令
public:
    SrsConfDirective();
    virtual ~SrsConfDirective();
public:
    // Deep copy the directive, for SrsConfig to use it to support reload in upyun cluster,
    // For when reload the upyun dynamic config, the root will be changed,
    // so need to copy it to an old root directive, and use the copy result to do reload.
    virtual SrsConfDirective* copy();
    // @param except the name of sub directive.
    //深度复制该指令，让SrsConfig使用它来支持在upyun集群中重新加载，
    //因为当重新加载upyun动态配置时，根目录将会被改变，
    //因此，需要将它复制到一个旧的根指令中，并使用复制结果来进行重新加载。
    virtual SrsConfDirective* copy(std::string except);
// args 参数
public:
    // Get the args0,1,2, if user want to get more args,
    // directly use the args.at(index).
    // 从指令数组args中获取第几个指令
    virtual std::string arg0();
    virtual std::string arg1();
    virtual std::string arg2();
    virtual std::string arg3();
// directives 子指令数组
public:
    // Get the directive by index.
    // @remark, assert the index<directives.size().
    // 从子指令数组directives中获取第index个指令
    virtual SrsConfDirective* at(int index);
    // Get the directive by name, return the first match.
    // 按名称获取指令，返回第一个匹配项。
    virtual SrsConfDirective* get(std::string _name);
    // Get the directive by name and its arg0, return the first match.
    //获取指令及其第一个参数arg0，返回第一个匹配。
    virtual SrsConfDirective* get(std::string _name, std::string _arg0);
    // RAW 
public:
    virtual SrsConfDirective* get_or_create(std::string n);
    virtual SrsConfDirective* get_or_create(std::string n, std::string a0);
    virtual SrsConfDirective* get_or_create(std::string n, std::string a0, std::string a1);
    virtual SrsConfDirective* set_arg0(std::string a0);
    // Remove the v from sub directives, user must free the v.
    // 从子指令数组directives中删除一个指令v，用户自己管理释放v
    virtual void remove(SrsConfDirective* v);
// help utilities
public:
    // Whether current directive is vhost.
    // 当前指令是不是vhost
    virtual bool is_vhost();
    // Whether current directive is stream_caster.
    // 当前指令是不是stream_caster
    virtual bool is_stream_caster();
// Parse utilities
public:
    // Parse config directive from file buffer.
    // 从配置缓冲区中解析指令
    virtual srs_error_t parse(srs_internal::SrsConfigBuffer* buffer, SrsConfig* conf = NULL);
    // Marshal the directive to writer.
    // @param level, the root is level0, all its directives are level1, and so on.
    // 将指令写入writer，level是级别，根指令是level0，根指令的所有子指令都是level1，以此类推。
    virtual srs_error_t persistence(SrsFileWriter* writer, int level);
    // Dumps the args[0-N] to array(string).
    // 将参数转储到JSON数组（字符串）中。
    virtual SrsJsonArray* dumps_args();
    // Dumps arg0 to str, number or boolean.
    // 把第一个参数arg0转换为字符串，数字，或布尔类型
    virtual SrsJsonAny* dumps_arg0_to_str();
    virtual SrsJsonAny* dumps_arg0_to_integer();
    virtual SrsJsonAny* dumps_arg0_to_number();
    virtual SrsJsonAny* dumps_arg0_to_boolean();
// private parse.
private:
    // The directive parsing context.指令解析上下文
    enum SrsDirectiveContext {
        // The root directives, parsing file.
        // 根指令，解析文件。
        SrsDirectiveContextFile,
        // For each directive, parsing text block.
        // 对每个指令，解析文本
        SrsDirectiveContextBlock,
    };
    enum SrsDirectiveState {//指令解析状态
        // Init state
        SrsDirectiveStateInit,
        // The directive terminated by ';' found
        SrsDirectiveStateEntire,
        // The token terminated by '{' found
        SrsDirectiveStateBlockStart,
        // The '}' found
        SrsDirectiveStateBlockEnd,
        // The config file is done
        SrsDirectiveStateEOF,
    };
    // Parse the conf from buffer. the work flow:
    // 1. read a token(directive args and a ret flag),
    // 2. initialize the directive by args, args[0] is name, args[1-N] is args of directive,
    // 3. if ret flag indicates there are child-directives, read_conf(directive, block) recursively.
    //从缓冲区解析conf。工作流程：
    //1. 读取一个标记（指令args和一个ret标志），
    //2. 通过args初始化指令，args[0]是名称，args[1-N]是指令的args，
    //3. 如果ret标志表示有子指令，则read_conf（指令，块）递归化。
    virtual srs_error_t parse_conf(srs_internal::SrsConfigBuffer* buffer, SrsDirectiveContext ctx, SrsConfig* conf);
    // Read a token from buffer.
    // A token, is the directive args and a flag indicates whether has child-directives.
    // @param args, the output directive args, the first is the directive name, left is the args.
    // @param line_start, the actual start line of directive.
    // @return, an error code indicates error or has child-directives.
    // 从缓冲区中读取一个标记。
    // 一个标记，是指令参数，并有一个标志指示是否有子指令。
    // @param args,输出指令的args，第一个是指令名，左边是args。
    // @param line_start,指令的实际起始行。
    // @return,错误代码指示错误或有子指令。
    virtual srs_error_t read_token(srs_internal::SrsConfigBuffer* buffer, std::vector<std::string>& args, int& line_start, SrsDirectiveState& state);
};

// The config service provider.
// For the config supports reload, so never keep the reference cross st-thread,
// that is, never save the SrsConfDirective* get by any api of config,
// For it maybe free in the reload st-thread cycle.
// You could keep it before st-thread switch, or simply never keep it.
// 提供配置服务，因为配置支持重新加载，不要在多协程中使用SrsConfDirective的引用或保存其指针，因为其可能在重新加载时被释放
// 可以在st线程切换之前保存它，或者干脆永远不要保存它。
class SrsConfig
{
    friend class SrsConfDirective;
// user command
private:
    // Whether show help and exit.是否显示帮助并退出
    bool show_help;
    // Whether test config file and exit.是否测试配置文件并退出
    bool test_conf;
    // Whether show SRS version and exit.是否显示SRS版本并退出
    bool show_version;
    // Whether show SRS signature and exit.是否显示SRS签名并退出
    bool show_signature;
    // Whether only use environment variable, ignore config file.
    // Set it by argv "-e" or env "SRS_ENV_ONLY=on".
    bool env_only_;//是否使用用户环境变量，忽略配置文件
// global env variables.
private://全局环境变量
    // The user parameters, the argc and argv.
    // The argv is " ".join(argv), where argv is from main(argc, argv).
    std::string _argv;//从main函数传参过来的argv参数组合
    // current working directory.
    std::string _cwd;//当前工作目录
    // Config section
private:
    // The last parsed config file.
    // If  reload, reload the config file.
    // 最后一次解析的配置文件名，重载会更新
    std::string config_file;
protected:
    // The directive root. 根指令
    SrsConfDirective* root;
// Reload  section
private:
    // The reload subscribers, when reload, callback all handlers.
    // 重新加载订阅集，在重新加载时，会回调所有处理程序。
    std::vector<ISrsReloadHandler*> subscribes;
public:
    SrsConfig();
    virtual ~SrsConfig();
// Reload
public:
    // For reload handler to register itself,
    // when config service do the reload, callback the handler.
    // 重载注册，当配置服务进行重新加载时，回调处理程序。
    virtual void subscribe(ISrsReloadHandler* handler);
    // For reload handler to unregister itself.
    // 取消重载注册
    virtual void unsubscribe(ISrsReloadHandler* handler);
    // Reload  the config file.
    // @remark, user can test the config before reload it.
    virtual srs_error_t reload();//重载配置文件

protected:
    // Reload  from the config. 从SrsConfig中重载配置
    // @remark, use protected for the utest to override with mock.
    virtual srs_error_t reload_conf(SrsConfig* conf);

// Parse options and file
public:
    // Parse the cli, the main(argc,argv) function.
    // 解析main函数传参
    virtual srs_error_t parse_options(int argc, char** argv);
    // initialize the cwd for server,
    // because we may change the workdir.
    // 初始化当前工作目录，因为可能会改变
    virtual srs_error_t initialize_cwd();
    // Marshal current config to file.
    // 将当前配置转换到文件。
    virtual srs_error_t persistence();
private:
    virtual srs_error_t do_persistence(SrsFileWriter* fw);
public:
    // Dumps the http_api sections to json for raw api info.

private:
    virtual srs_error_t do_reload_testwork();//chw
public:
    // Get the config file path.
    virtual std::string config();//获取配置文件路径
private:
    // Parse each argv.解析每一个参数
    virtual srs_error_t parse_argv(int& i, char** argv);
    // Print help and exit.打印帮助并退出
    virtual void print_help(char** argv);
public:
    // Parse the config file, which is specified by cli.
    // 解析由cli指定的配置文件。
    virtual srs_error_t parse_file(const char* filename);
private:
    // Build a buffer from a src, which is string content or filename.
    // 从src构建一个缓冲区，它是字符串内容或文件名。
    virtual srs_error_t build_buffer(std::string src, srs_internal::SrsConfigBuffer** pbuffer);
public:
    // Check the parsed config. 检查配置文件
    virtual srs_error_t check_config();
protected:
    virtual srs_error_t check_normal_config();
    virtual srs_error_t check_number_connections();
protected:
    // Parse config from the buffer.    从缓冲区解析配置
    // @param buffer, the config buffer, user must delete it.
    // @remark, use protected for the utest to override with mock.
    virtual srs_error_t parse_buffer(srs_internal::SrsConfigBuffer* buffer);
    // global env
public:
    // Get the current work directory. 获取当前工作目录
    virtual std::string cwd();
    // Get the cli, the main(argc,argv), program start command.
    // 获取main函数的传参
    virtual std::string argv();
// global section
public:
    // Get the directive root, corresponding to the config file.
    // The root directive, no name and args, contains directives.
    // All directive parsed can retrieve from root.
    //获取对应于配置文件的指令根目录。
    //根指令，没有名称和根参数，包含指令。
    //所有被解析的指令都可以从根目录中检索。
    virtual SrsConfDirective* get_root();
    // Get the daemon config.
    // If  true, SRS will run in daemon mode, fork and fork to reap the
    // grand-child process to init process.
    //获取守护进程配置。
    //如果为真，SRS将在守护进程模式、fork和fork中运行，以获取孙子进程到init进程。
    virtual bool get_daemon();
    // Whether srs in docker.
    virtual bool get_in_docker();
private:
    // Whether user use full.conf
    virtual bool is_full_config();
public:
    // Get the server id, generated a random one if not configured.
    // 获取服务器id，如果未配置，则生成一个随机的服务器id。
    virtual std::string get_server_id();
    // Get the max connections limit of system.
    // If  exceed the max connection, SRS will disconnect the connection.
    // @remark, linux will limit the connections of each process,
    //       for example, when you need SRS to service 10000+ connections,
    //       user must use "ulimit -HSn 10000" and config the max connections
    //       of SRS.
    //获取系统的最大连接限制。如果超过最大连接，SRS将断开连接。
    virtual int get_max_connections();
    // Get the listen port of SRS.
    // user can specifies multiple listen ports,
    // each args of directive is a listen port.
    //获取SRS的监听端口。
    //用户可以指定多个监听端口，
    //指令的每个参数都是一个侦听端口。
    virtual std::vector<std::string> get_listens();
    // Get the pid file path.
    // The pid file is used to save the pid of SRS,
    // use file lock to prevent multiple SRS starting.
    // @remark, if user need to run multiple SRS instance,
    //       for example, to start multiple SRS for multiple CPUs,
    //       user can use different pid file for each process.
    //获取pid文件路径。
    //pid文件用于保存SRS的pid，
    //使用文件锁定来防止多个SRS启动。
    virtual std::string get_pid_file();
    // Get pithy print pulse in srs_utime_t,
    // For example, all rtmp connections only print one message
    // every this interval in ms.
    // 在srs_utime_t中获得简洁的打印脉冲，例如，所有rtmp连接在ms内每此间隔只打印一条消息。
    virtual srs_utime_t get_pithy_print();
    // Whether use utc-time to format the time.
    // 是否使用utc时间来格式化时间。
    virtual bool get_utc_time();
    // Get the configed work dir.
    // ignore if empty string.
    virtual std::string get_work_dir();
    // Whether use asprocess mode.
    virtual bool get_asprocess();
    // Whether query the latest available version of SRS.
    virtual bool whether_query_latest_version();
private:
    SrsConfDirective* get_srt(std::string vhost);
public:
    bool get_srt_enabled(std::string vhost);
    bool get_srt_to_rtmp(std::string vhost);
public:
    // Whether log to file.
    virtual bool get_log_tank_file();
    // Get the log level.
    virtual std::string get_log_level();
    virtual std::string get_log_level_v2();
    // Get the log file path.
    virtual std::string get_log_file();
    // Whether ffmpeg log enabled
    virtual bool get_ff_log_enabled();
    // The ffmpeg log dir.
    // @remark, /dev/null to disable it.
    virtual std::string get_ff_log_dir();
    // The ffmpeg log level.
    virtual std::string get_ff_log_level();
// The MPEG-DASH section.

    virtual std::string get_http_server_listen();
private:
private:
    SrsConfDirective* get_https_api();
};

#endif


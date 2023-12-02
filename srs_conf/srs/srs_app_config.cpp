#include <srs_app_config.hpp>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
// file operations.
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef __linux__
#include <linux/version.h>
#include <sys/utsname.h>
#endif

#include <vector>
#include <algorithm>
using namespace std;

#include <srs_kernel_error.hpp>
#include <srs_kernel_file.hpp>
#include <srs_protocol_json.hpp>


using namespace srs_internal;

// @global the version to identify the core.
const char* _srs_version = "XCORE-" RTMP_SIG_SRS_SERVER;

// when user config an invalid value, macros to perfer true or false.
#define SRS_CONF_PERFER_FALSE(conf_arg) conf_arg == "on"
#define SRS_CONF_PERFER_TRUE(conf_arg) conf_arg != "off"

// default config file.
#define SRS_DEFAULT_CONFIG "srs.conf"
#define SRS_CONF_DEFAULT_COFNIG_FILE SRS_DEFAULT_CONFIG

// '\n'
#define SRS_LF (char)SRS_CONSTS_LF

// '\r'
#define SRS_CR (char)SRS_CONSTS_CR

// Overwrite the config by env.
// 使用环境变量覆盖配置
#define SRS_OVERWRITE_BY_ENV_STRING(key) if (!srs_getenv(key).empty()) return srs_getenv(key)
#define SRS_OVERWRITE_BY_ENV_BOOL(key) if (!srs_getenv(key).empty()) return SRS_CONF_PERFER_FALSE(srs_getenv(key))
#define SRS_OVERWRITE_BY_ENV_BOOL2(key) if (!srs_getenv(key).empty()) return SRS_CONF_PERFER_TRUE(srs_getenv(key))
#define SRS_OVERWRITE_BY_ENV_INT(key) if (!srs_getenv(key).empty()) return ::atoi(srs_getenv(key).c_str())
#define SRS_OVERWRITE_BY_ENV_FLOAT(key) if (!srs_getenv(key).empty()) return ::atof(srs_getenv(key).c_str())
#define SRS_OVERWRITE_BY_ENV_SECONDS(key) if (!srs_getenv(key).empty()) return srs_utime_t(::atoi(srs_getenv(key).c_str()) * SRS_UTIME_SECONDS)
#define SRS_OVERWRITE_BY_ENV_MILLISECONDS(key) if (!srs_getenv(key).empty()) return (srs_utime_t)(::atoi(srs_getenv(key).c_str()) * SRS_UTIME_MILLISECONDS)
#define SRS_OVERWRITE_BY_ENV_FLOAT_SECONDS(key) if (!srs_getenv(key).empty()) return srs_utime_t(::atof(srs_getenv(key).c_str()) * SRS_UTIME_SECONDS)
#define SRS_OVERWRITE_BY_ENV_FLOAT_MILLISECONDS(key) if (!srs_getenv(key).empty()) return srs_utime_t(::atof(srs_getenv(key).c_str()) * SRS_UTIME_MILLISECONDS)
#define SRS_OVERWRITE_BY_ENV_DIRECTIVE(key) { \
        static SrsConfDirective* dir = NULL;      \
        if (!dir && !srs_getenv(key).empty()) {   \
            string v = srs_getenv(key);           \
            dir = new SrsConfDirective();         \
            dir->name = key;                      \
            dir->args.push_back(v);               \
        }                                         \
        if (dir) return dir;                      \
    }

/**
 * whether the ch is common space.
 */
bool is_common_space(char ch)
{
    return (ch == ' ' || ch == '\t' || ch == SRS_CR || ch == SRS_LF);
}

namespace srs_internal
{
    SrsConfigBuffer::SrsConfigBuffer()
    {
        line = 1;
        
        pos = last = start = NULL;
        end = start;
    }
    
    SrsConfigBuffer::~SrsConfigBuffer()
    {
        srs_freepa(start);
    }

    // LCOV_EXCL_START
    srs_error_t SrsConfigBuffer::fullfill(const char* filename)
    {
        srs_error_t err = srs_success;
        
        SrsFileReader reader;
        
        // open file reader.
        if ((err = reader.open(filename)) != srs_success) {
            return srs_error_wrap(err, "open file=%s", filename);
        }
        
        // read all.
        int filesize = (int)reader.filesize();
        
        // create buffer
        srs_freepa(start);
        pos = last = start = new char[filesize];
        end = start + filesize;
        
        // read total content from file.
        ssize_t nread = 0;
        if ((err = reader.read(start, filesize, &nread)) != srs_success) {
            return srs_error_wrap(err, "read %d only %d bytes", filesize, (int)nread);
        }
        
        return err;
    }
    // LCOV_EXCL_STOP
    
    bool SrsConfigBuffer::empty()
    {
        return pos >= end;
    }
};

bool srs_directive_equals_self(SrsConfDirective* a, SrsConfDirective* b)
{
    // both NULL, equal.
    if (!a && !b) {
        return true;
    }
    
    if (!a || !b) {
        return false;
    }
    
    if (a->name != b->name) {
        return false;
    }
    
    if (a->args.size() != b->args.size()) {
        return false;
    }
    
    for (int i = 0; i < (int)a->args.size(); i++) {
        if (a->args.at(i) != b->args.at(i)) {
            return false;
        }
    }
    
    if (a->directives.size() != b->directives.size()) {
        return false;
    }
    
    return true;
}

bool srs_directive_equals(SrsConfDirective* a, SrsConfDirective* b)
{
    // both NULL, equal.
    if (!a && !b) {
        return true;
    }
    
    if (!srs_directive_equals_self(a, b)) {
        return false;
    }
    
    for (int i = 0; i < (int)a->directives.size(); i++) {
        SrsConfDirective* a0 = a->at(i);
        SrsConfDirective* b0 = b->at(i);
        
        if (!srs_directive_equals(a0, b0)) {
            return false;
        }
    }
    
    return true;
}

bool srs_directive_equals(SrsConfDirective* a, SrsConfDirective* b, string except)
{
    // both NULL, equal.
    if (!a && !b) {
        return true;
    }
    
    if (!srs_directive_equals_self(a, b)) {
        return false;
    }
    
    for (int i = 0; i < (int)a->directives.size(); i++) {
        SrsConfDirective* a0 = a->at(i);
        SrsConfDirective* b0 = b->at(i);
        
        // donot compare the except child directive.
        if (a0->name == except) {
            continue;
        }
        
        if (!srs_directive_equals(a0, b0, except)) {
            return false;
        }
    }
    
    return true;
}

void set_config_directive(SrsConfDirective* parent, string dir, string value)
{
    SrsConfDirective* d = parent->get_or_create(dir);
    d->name = dir;
    d->args.clear();
    d->args.push_back(value);
}

bool srs_config_hls_is_on_error_ignore(string strategy)
{
    return strategy == "ignore";
}

bool srs_config_hls_is_on_error_continue(string strategy)
{
    return strategy == "continue";
}

bool srs_config_ingest_is_file(string type)
{
    return type == "file";
}

bool srs_config_ingest_is_stream(string type)
{
    return type == "stream";
}

bool srs_config_dvr_is_plan_segment(string plan)
{
    return plan == "segment";
}

bool srs_config_dvr_is_plan_session(string plan)
{
    return plan == "session";
}

bool srs_stream_caster_is_udp(string caster)
{
    return caster == "mpegts_over_udp";
}

bool srs_stream_caster_is_flv(string caster)
{
    return caster == "flv";
}

bool srs_stream_caster_is_gb28181(string caster)
{
    return caster == "gb28181";
}

string srs_config_bool2switch(string sbool)
{
    return sbool == "true"? "on":"off";
}

SrsConfDirective::SrsConfDirective()
{
    conf_line = 0;
}

SrsConfDirective::~SrsConfDirective()
{
    std::vector<SrsConfDirective*>::iterator it;
    for (it = directives.begin(); it != directives.end(); ++it) {
        SrsConfDirective* directive = *it;
        srs_freep(directive);
    }
    directives.clear();
}

SrsConfDirective* SrsConfDirective::copy()
{
    return copy("");
}

SrsConfDirective* SrsConfDirective::copy(string except)
{
    SrsConfDirective* cp = new SrsConfDirective();
    
    cp->conf_line = conf_line;
    cp->name = name;
    cp->args = args;
    
    for (int i = 0; i < (int)directives.size(); i++) {
        SrsConfDirective* directive = directives.at(i);
        if (!except.empty() && directive->name == except) {
            continue;
        }
        cp->directives.push_back(directive->copy(except));
    }
    
    return cp;
}

string SrsConfDirective::arg0()
{
    if (args.size() > 0) {
        return args.at(0);
    }
    
    return "";
}

string SrsConfDirective::arg1()
{
    if (args.size() > 1) {
        return args.at(1);
    }
    
    return "";
}

string SrsConfDirective::arg2()
{
    if (args.size() > 2) {
        return args.at(2);
    }
    
    return "";
}

string SrsConfDirective::arg3()
{
    if (args.size() > 3) {
        return args.at(3);
    }
    
    return "";
}

SrsConfDirective* SrsConfDirective::at(int index)
{
    srs_assert(index < (int)directives.size());
    return directives.at(index);
}

SrsConfDirective* SrsConfDirective::get(string _name)
{
    std::vector<SrsConfDirective*>::iterator it;
    for (it = directives.begin(); it != directives.end(); ++it) {
        SrsConfDirective* directive = *it;
        if (directive->name == _name) {
            return directive;
        }
    }
    
    return NULL;
}

SrsConfDirective* SrsConfDirective::get(string _name, string _arg0)
{
    std::vector<SrsConfDirective*>::iterator it;
    for (it = directives.begin(); it != directives.end(); ++it) {
        SrsConfDirective* directive = *it;
        if (directive->name == _name && directive->arg0() == _arg0) {
            return directive;
        }
    }
    
    return NULL;
}

SrsConfDirective* SrsConfDirective::get_or_create(string n)
{
    SrsConfDirective* conf = get(n);
    
    if (!conf) {
        conf = new SrsConfDirective();
        conf->name = n;
        directives.push_back(conf);
    }
    
    return conf;
}

SrsConfDirective* SrsConfDirective::get_or_create(string n, string a0)
{
    SrsConfDirective* conf = get(n, a0);
    
    if (!conf) {
        conf = new SrsConfDirective();
        conf->name = n;
        conf->args.push_back(a0);
        directives.push_back(conf);
    }
    
    return conf;
}

SrsConfDirective* SrsConfDirective::get_or_create(string n, string a0, string a1)
{
    SrsConfDirective* conf = get(n, a0);

    if (!conf) {
        conf = new SrsConfDirective();
        conf->name = n;
        conf->args.push_back(a0);
        conf->args.push_back(a1);
        directives.push_back(conf);
    }

    return conf;
}

SrsConfDirective* SrsConfDirective::set_arg0(string a0)
{
    if (arg0() == a0) {
        return this;
    }
    
    // update a0.
    if (!args.empty()) {
        args.erase(args.begin());
    }
    
    args.insert(args.begin(), a0);
    
    return this;
}

void SrsConfDirective::remove(SrsConfDirective* v)
{
    std::vector<SrsConfDirective*>::iterator it;
    if ((it = std::find(directives.begin(), directives.end(), v)) != directives.end()) {
        it = directives.erase(it);
    }
}

bool SrsConfDirective::is_vhost()
{
    return name == "vhost";
}

bool SrsConfDirective::is_stream_caster()
{
    return name == "stream_caster";
}

srs_error_t SrsConfDirective::parse(SrsConfigBuffer* buffer, SrsConfig* conf)
{
    return parse_conf(buffer, SrsDirectiveContextFile, conf);
}

srs_error_t SrsConfDirective::persistence(SrsFileWriter* writer, int level)
{
    srs_error_t err = srs_success;
    
    static char SPACE = SRS_CONSTS_SP;
    static char SEMICOLON = SRS_CONSTS_SE;
    static char LF = SRS_CONSTS_LF;
    static char LB = SRS_CONSTS_LB;
    static char RB = SRS_CONSTS_RB;
    static const char* INDENT = "    ";
    
    // for level0 directive, only contains sub directives.
    if (level > 0) {
        // indent by (level - 1) * 4 space.
        for (int i = 0; i < level - 1; i++) {
            if ((err = writer->write((char*)INDENT, 4, NULL)) != srs_success) {
                return srs_error_wrap(err, "write indent");
            }
        }
        
        // directive name.
        if ((err = writer->write((char*)name.c_str(), (int)name.length(), NULL)) != srs_success) {
            return srs_error_wrap(err, "write name");
        }
        if (!args.empty() && (err = writer->write((char*)&SPACE, 1, NULL)) != srs_success) {
            return srs_error_wrap(err, "write name space");
        }
        
        // directive args.
        for (int i = 0; i < (int)args.size(); i++) {
            std::string& arg = args.at(i);
            if ((err = writer->write((char*)arg.c_str(), (int)arg.length(), NULL)) != srs_success) {
                return srs_error_wrap(err, "write arg");
            }
            if (i < (int)args.size() - 1 && (err = writer->write((char*)&SPACE, 1, NULL)) != srs_success) {
                return srs_error_wrap(err, "write arg space");
            }
        }
        
        // native directive, without sub directives.
        if (directives.empty()) {
            if ((err = writer->write((char*)&SEMICOLON, 1, NULL)) != srs_success) {
                return srs_error_wrap(err, "write arg semicolon");
            }
        }
    }
    
    // persistence all sub directives.
    if (level > 0) {
        if (!directives.empty()) {
            if ((err = writer->write((char*)&SPACE, 1, NULL)) != srs_success) {
                return srs_error_wrap(err, "write sub-dir space");
            }
            if ((err = writer->write((char*)&LB, 1, NULL)) != srs_success) {
                return srs_error_wrap(err, "write sub-dir left-brace");
            }
        }
        
        if ((err = writer->write((char*)&LF, 1, NULL)) != srs_success) {
            return srs_error_wrap(err, "write sub-dir linefeed");
        }
    }
    
    for (int i = 0; i < (int)directives.size(); i++) {
        SrsConfDirective* dir = directives.at(i);
        if ((err = dir->persistence(writer, level + 1)) != srs_success) {
            return srs_error_wrap(err, "sub-dir %s", dir->name.c_str());
        }
    }
    
    if (level > 0 && !directives.empty()) {
        // indent by (level - 1) * 4 space.
        for (int i = 0; i < level - 1; i++) {
            if ((err = writer->write((char*)INDENT, 4, NULL)) != srs_success) {
                return srs_error_wrap(err, "write sub-dir indent");
            }
        }
        
        if ((err = writer->write((char*)&RB, 1, NULL)) != srs_success) {
            return srs_error_wrap(err, "write sub-dir right-brace");
        }
        
        if ((err = writer->write((char*)&LF, 1, NULL)) != srs_success) {
            return srs_error_wrap(err, "write sub-dir linefeed");
        }
    }
    
    
    return err;
}

// LCOV_EXCL_START
SrsJsonArray* SrsConfDirective::dumps_args()
{
    SrsJsonArray* arr = SrsJsonAny::array();
    for (int i = 0; i < (int)args.size(); i++) {
        string arg = args.at(i);
        arr->append(SrsJsonAny::str(arg.c_str()));
    }
    return arr;
}

SrsJsonAny* SrsConfDirective::dumps_arg0_to_str()
{
    return SrsJsonAny::str(arg0().c_str());
}

SrsJsonAny* SrsConfDirective::dumps_arg0_to_integer()
{
    return SrsJsonAny::integer(::atoll(arg0().c_str()));
}

SrsJsonAny* SrsConfDirective::dumps_arg0_to_number()
{
    return SrsJsonAny::number(::atof(arg0().c_str()));
}

SrsJsonAny* SrsConfDirective::dumps_arg0_to_boolean()
{
    return SrsJsonAny::boolean(arg0() == "on");
}
// LCOV_EXCL_STOP

// see: ngx_conf_parse
srs_error_t SrsConfDirective::parse_conf(SrsConfigBuffer* buffer, SrsDirectiveContext ctx, SrsConfig* conf)
{
    srs_error_t err = srs_success;
    
    while (true) {
        std::vector<string> args;
        int line_start = 0;
        SrsDirectiveState state = SrsDirectiveStateInit;
        if ((err = read_token(buffer, args, line_start, state)) != srs_success) {
            return srs_error_wrap(err, "read token, line=%d, state=%d", line_start, state);
        }

        if (state == SrsDirectiveStateBlockEnd) {
            return ctx == SrsDirectiveContextBlock ? srs_success : srs_error_new(ERROR_SYSTEM_CONFIG_INVALID, "line %d: unexpected \"}\"", buffer->line);
        }
        if (state == SrsDirectiveStateEOF) {
            return ctx != SrsDirectiveContextBlock ? srs_success : srs_error_new(ERROR_SYSTEM_CONFIG_INVALID, "line %d: unexpected end of file, expecting \"}\"", conf_line);
        }
        if (args.empty()) {
            return srs_error_new(ERROR_SYSTEM_CONFIG_INVALID, "line %d: empty directive", conf_line);
        }
        
        // Build normal directive which is not "include".
        if (args.at(0) != "include") {
            SrsConfDirective* directive = new SrsConfDirective();

            directive->conf_line = line_start;
            directive->name = args[0];
            args.erase(args.begin());
            directive->args.swap(args);

            directives.push_back(directive);

            if (state == SrsDirectiveStateBlockStart) {
                if ((err = directive->parse_conf(buffer, SrsDirectiveContextBlock, conf)) != srs_success) {
                    return srs_error_wrap(err, "parse dir");
                }
            }
            continue;
        }

        // Parse including, allow multiple files.
        vector<string> files(args.begin() + 1, args.end());
        if (files.empty()) {
            return srs_error_new(ERROR_SYSTEM_CONFIG_INVALID, "line %d: include is empty directive", buffer->line);
        }
        if (!conf) {
            return srs_error_new(ERROR_SYSTEM_CONFIG_INVALID, "line %d: no config", buffer->line);
        }

        for (int i = 0; i < (int)files.size(); i++) {
            std::string file = files.at(i);
            srs_assert(!file.empty());
            printf("config parse include %s", file.c_str());

            SrsConfigBuffer* include_file_buffer = NULL;
            SrsAutoFree(SrsConfigBuffer, include_file_buffer);
            if ((err = conf->build_buffer(file, &include_file_buffer)) != srs_success) {
                return srs_error_wrap(err, "buffer fullfill %s", file.c_str());
            }

            if ((err = parse_conf(include_file_buffer, SrsDirectiveContextFile, conf)) != srs_success) {
                return srs_error_wrap(err, "parse include buffer");
            }
        }
    }
    
    return err;
}

// see: ngx_conf_read_token
srs_error_t SrsConfDirective::read_token(SrsConfigBuffer* buffer, vector<string>& args, int& line_start, SrsDirectiveState& state)
{
    srs_error_t err = srs_success;
    
    char* pstart = buffer->pos;
    
    bool sharp_comment = false;
    
    bool d_quoted = false;
    bool s_quoted = false;
    
    bool need_space = false;
    bool last_space = true;
    
    while (true) {
        if (buffer->empty()) {
            if (!args.empty() || !last_space) {
                return srs_error_new(ERROR_SYSTEM_CONFIG_INVALID,
                    "line %d: unexpected end of file, expecting ; or \"}\"",
                    buffer->line);
            }
            printf("config parse complete\n");

            state = SrsDirectiveStateEOF;
            return err;
        }
        
        char ch = *buffer->pos++;
        
        if (ch == SRS_LF) {
            buffer->line++;
            sharp_comment = false;
        }
        
        if (sharp_comment) {
            continue;
        }
        
        if (need_space) {
            if (is_common_space(ch)) {
                last_space = true;
                need_space = false;
                continue;
            }
            if (ch == ';') {
                state = SrsDirectiveStateEntire;
                return err;
            }
            if (ch == '{') {
                state = SrsDirectiveStateBlockStart;
                return err;
            }
            return srs_error_new(ERROR_SYSTEM_CONFIG_INVALID, "line %d: unexpected '%c'", buffer->line, ch);
        }
        
        // last charecter is space.
        if (last_space) {
            if (is_common_space(ch)) {
                continue;
            }
            pstart = buffer->pos - 1;
            switch (ch) {
                case ';':
                    if (args.size() == 0) {
                        return srs_error_new(ERROR_SYSTEM_CONFIG_INVALID, "line %d: unexpected ';'", buffer->line);
                    }
                    state = SrsDirectiveStateEntire;
                    return err;
                case '{':
                    if (args.size() == 0) {
                        return srs_error_new(ERROR_SYSTEM_CONFIG_INVALID, "line %d: unexpected '{'", buffer->line);
                    }
                    state = SrsDirectiveStateBlockStart;
                    return err;
                case '}':
                    if (args.size() != 0) {
                        return srs_error_new(ERROR_SYSTEM_CONFIG_INVALID, "line %d: unexpected '}'", buffer->line);
                    }
                    state = SrsDirectiveStateBlockEnd;
                    return err;
                case '#':
                    sharp_comment = 1;
                    continue;
                case '"':
                    pstart++;
                    d_quoted = true;
                    last_space = 0;
                    continue;
                case '\'':
                    pstart++;
                    s_quoted = true;
                    last_space = 0;
                    continue;
                default:
                    last_space = 0;
                    continue;
            }
        } else {
            // last charecter is not space
            if (line_start == 0) {
                line_start = buffer->line;
            }
            
            bool found = false;
            if (d_quoted) {
                if (ch == '"') {
                    d_quoted = false;
                    need_space = true;
                    found = true;
                }
            } else if (s_quoted) {
                if (ch == '\'') {
                    s_quoted = false;
                    need_space = true;
                    found = true;
                }
            } else if (is_common_space(ch) || ch == ';' || ch == '{') {
                last_space = true;
                found = 1;
            }
            
            if (found) {
                int len = (int)(buffer->pos - pstart);
                char* aword = new char[len];
                memcpy(aword, pstart, len);
                aword[len - 1] = 0;
                
                string word_str = aword;
                if (!word_str.empty()) {
                    args.push_back(word_str);
                }
                srs_freepa(aword);
                
                if (ch == ';') {
                    state = SrsDirectiveStateEntire;
                    return err;
                }
                if (ch == '{') {
                    state = SrsDirectiveStateBlockStart;
                    return err;
                }
            }
        }
    }
    
    return err;
}

SrsConfig::SrsConfig()
{
    env_only_ = false;
    
    show_help = false;
    show_version = false;
    test_conf = false;
    show_signature = false;
    
    root = new SrsConfDirective();
    root->conf_line = 0;
    root->name = "root";
}

SrsConfig::~SrsConfig()
{
    srs_freep(root);
}

void SrsConfig::subscribe(ISrsReloadHandler* handler)
{
    std::vector<ISrsReloadHandler*>::iterator it;
    
    it = std::find(subscribes.begin(), subscribes.end(), handler);
    if (it != subscribes.end()) {
        return;
    }
    
    subscribes.push_back(handler);
}

void SrsConfig::unsubscribe(ISrsReloadHandler* handler)
{
    std::vector<ISrsReloadHandler*>::iterator it;
    
    it = std::find(subscribes.begin(), subscribes.end(), handler);
    if (it == subscribes.end()) {
        return;
    }
    
    it = subscribes.erase(it);
}

// LCOV_EXCL_START
srs_error_t SrsConfig::reload()
{
    srs_error_t err = srs_success;
    
    SrsConfig conf;
    
    if ((err = conf.parse_file(config_file.c_str())) != srs_success) {
        return srs_error_wrap(err, "parse file");
    }
    printf("config reloader parse file success.\n");
    
    // transform config to compatible with previous style of config.
//    if ((err = srs_config_transform_vhost(conf.root)) != srs_success) {
//        return srs_error_wrap(err, "transform config");
//    }
    
    if ((err = conf.check_config()) != srs_success) {
        return srs_error_wrap(err, "check config");
    }
    
    if ((err = reload_conf(&conf)) != srs_success) {
        return srs_error_wrap(err, "reload config");
    }
    
    return err;
}
// LCOV_EXCL_STOP



srs_error_t SrsConfig::reload_conf(SrsConfig* conf)
{
    srs_error_t err = srs_success;
    
    SrsConfDirective* old_root = root;
    SrsAutoFree(SrsConfDirective, old_root);
    
    root = conf->root;
    conf->root = NULL;
    
    // never support reload:
    //      daemon
    //
    // always support reload without additional code:
    //      chunk_size, ff_log_dir,
    //      http_hooks, heartbeat,
    //      security

    if ((err = do_reload_testwork()) != srs_success) {
        return srs_error_wrap(err, "testwork");;
    }

    return err;
}


// see: ngx_get_options
// LCOV_EXCL_START
srs_error_t SrsConfig::parse_options(int argc, char** argv)
{
    srs_error_t err = srs_success;
    
    // argv
    for (int i = 0; i < argc; i++) {
        _argv.append(argv[i]);
        
        if (i < argc - 1) {
            _argv.append(" ");
        }
    }
    
    // Show help if it has no argv
    show_help = argc == 1;
    for (int i = 1; i < argc; i++) {
        if ((err = parse_argv(i, argv)) != srs_success) {
            return srs_error_wrap(err, "parse argv");
        }
    }
    
    if (show_help) {
        print_help(argv);
        exit(0);
    }
    
    if (show_version) {
        fprintf(stdout, "%s\n", RTMP_SIG_SRS_VERSION);
        exit(0);
    }
    if (show_signature) {
        fprintf(stdout, "%s\n", RTMP_SIG_SRS_SERVER);
        exit(0);
    }

    // The first hello message.
    printf(_srs_version);

    // Config the env_only_ by env.
    if (getenv("SRS_ENV_ONLY")) env_only_ = true;

    // Overwrite the config by env SRS_CONFIG_FILE.
    if (!env_only_ && !srs_getenv("srs.config.file").empty()) { // SRS_CONFIG_FILE
        string ov = config_file; config_file = srs_getenv("srs.config.file");
        printf("ENV: Overwrite config %s to %s", ov.c_str(), config_file.c_str());
    }

    // Make sure config file exists.
    if (!env_only_ && !srs_path_exists(config_file)) {
        return srs_error_new(ERROR_SYSTEM_CONFIG_INVALID, "no config file at %s", config_file.c_str());
    }

    // Parse the matched config file.
    if (!env_only_) {
        err = parse_file(config_file.c_str());
    }

    if (test_conf) {
        // the parse_file never check the config,
        // we check it when user requires check config file.
//        if (err == srs_success && (err = srs_config_transform_vhost(root)) == srs_success) {
//            if ((err = check_config()) == srs_success) {
//                printf("the config file %s syntax is ok", config_file.c_str());
//                printf("config file %s test is successful", config_file.c_str());
//                exit(0);
//            }
//        }

        printf("invalid config%s in %s", srs_error_summary(err).c_str(), config_file.c_str());
        printf("config file %s test is failed", config_file.c_str());
        exit(srs_error_code(err));
    }

    if (err != srs_success) {
        return srs_error_wrap(err, "invalid config");
    }

    // transform config to compatible with previous style of config.
    // 将配置转换为兼容以前的配置样式。
//    if ((err = srs_config_transform_vhost(root)) != srs_success) {
//        return srs_error_wrap(err, "transform");
//    }

    // If use env only, we set change to daemon(off) and console log.
    if (env_only_) {
        if (!getenv("SRS_DAEMON")) setenv("SRS_DAEMON", "off", 1);
        if (!getenv("SRS_SRS_LOG_TANK") && !getenv("SRS_LOG_TANK")) setenv("SRS_SRS_LOG_TANK", "console", 1);
        if (root->directives.empty()) root->get_or_create("vhost", "__defaultVhost__");
    }

    ////////////////////////////////////////////////////////////////////////
    // check log name and level
    ////////////////////////////////////////////////////////////////////////
    if (true) {
        std::string log_filename = this->get_log_file();
        if (get_log_tank_file() && log_filename.empty()) {
            return srs_error_new(ERROR_SYSTEM_CONFIG_INVALID, "no log file");
        }
        if (get_log_tank_file()) {
            printf("you can check log by: tail -n 30 -f %s\n", log_filename.c_str());
            printf("please check SRS by: ./etc/init.d/srs status\n");
        } else {
            printf("write log to console");
        }
    }
    
    return err;
}

srs_error_t SrsConfig::initialize_cwd()
{
    // cwd
    char cwd[256];
    getcwd(cwd, sizeof(cwd));
    _cwd = cwd;
    
    return srs_success;
}

srs_error_t SrsConfig::persistence()
{
    srs_error_t err = srs_success;
    
    // write to a tmp file, then mv to the config.
    std::string path = config_file + ".tmp";
    
    // open the tmp file for persistence
    SrsFileWriter fw;
    if ((err = fw.open(path)) != srs_success) {
        return srs_error_wrap(err, "open file");
    }
    
    // do persistence to writer.
    if ((err = do_persistence(&fw)) != srs_success) {
        ::unlink(path.c_str());
        return srs_error_wrap(err, "persistence");
    }
    
    // rename the config file.
    if (::rename(path.c_str(), config_file.c_str()) < 0) {
        ::unlink(path.c_str());
        return srs_error_new(ERROR_SYSTEM_CONFIG_PERSISTENCE, "rename %s=>%s", path.c_str(), config_file.c_str());
    }
    
    return err;
}

srs_error_t SrsConfig::do_persistence(SrsFileWriter* fw)
{
    srs_error_t err = srs_success;
    
    // persistence root directive to writer.
    if ((err = root->persistence(fw, 0)) != srs_success) {
        return srs_error_wrap(err, "root persistence");
    }
    
    return err;
}

srs_error_t SrsConfig::do_reload_testwork()
{
    srs_error_t err = srs_success;

    vector<ISrsReloadHandler*>::iterator it;
    for (it = subscribes.begin(); it != subscribes.end(); ++it) {
        ISrsReloadHandler* subscribe = *it;
        if ((err = subscribe->on_reload_testwork()) != srs_success) {
            return srs_error_wrap(err, "notify subscribes reload testwork failed");
        }
    }
    printf("reload testwork success.\n");

    return err;
}


string SrsConfig::config()
{
    return config_file;
}

// LCOV_EXCL_START
srs_error_t SrsConfig::parse_argv(int& i, char** argv)
{
    srs_error_t err = srs_success;
    
    char* p = argv[i];
    
    if (*p++ != '-') {
        show_help = true;
        return err;
    }
    
    while (*p) {
        switch (*p++) {
            case '?':
            case 'h':
                show_help = true;
                return err;
            case 't':
                show_help = false;
                test_conf = true;
                break;
            case 'e':
                show_help = false;
                env_only_ = true;
                break;
            case 'v':
            case 'V':
                show_help = false;
                show_version = true;
                return err;
            case 'g':
            case 'G':
                show_help = false;
                show_signature = true;
                break;
            case 'c':
                show_help = false;
                if (*p) {
                    config_file = p;
                    continue;
                }
                if (argv[++i]) {
                    config_file = argv[i];
                    continue;
                }
                return srs_error_new(ERROR_SYSTEM_CONFIG_INVALID, "-c requires params");
            case '-':
                continue;
            default:
                return srs_error_new(ERROR_SYSTEM_CONFIG_INVALID, "invalid option: \"%c\", read help: %s -h",
                    *(p - 1), argv[0]);
        }
    }
    
    return err;
}

void SrsConfig::print_help(char** argv)
{
    printf(
           "%s, %s, %s, created by %sand %s\n\n"
           "Usage: %s <-h?vVgGe>|<[-t] -c filename>\n"
           "Options:\n"
           "   -?, -h, --help      : Show this help and exit 0.\n"
           "   -v, -V, --version   : Show version and exit 0.\n"
           "   -g, -G              : Show server signature and exit 0.\n"
           "   -e                  : Use environment variable only, ignore config file.\n"
           "   -t                  : Test configuration file, exit with error code(0 for success).\n"
           "   -c filename         : Use config file to start server.\n"
           "For example:\n"
           "   %s -v\n"
           "   %s -t -c %s\n"
           "   %s -c %s\n",
           RTMP_SIG_SRS_SERVER, RTMP_SIG_SRS_URL, RTMP_SIG_SRS_LICENSE,
           RTMP_SIG_SRS_AUTHORS, SRS_CONSTRIBUTORS,
           argv[0], argv[0], argv[0], SRS_CONF_DEFAULT_COFNIG_FILE,
           argv[0], SRS_CONF_DEFAULT_COFNIG_FILE);
}

srs_error_t SrsConfig::parse_file(const char* filename)
{
    srs_error_t err = srs_success;
    
    config_file = filename;
    
    if (config_file.empty()) {
        return srs_error_new(ERROR_SYSTEM_CONFIG_INVALID, "empty config");
    }

    SrsConfigBuffer* buffer = NULL;
    SrsAutoFree(SrsConfigBuffer, buffer);
    if ((err = build_buffer(config_file, &buffer)) != srs_success) {
        return srs_error_wrap(err, "buffer fullfill %s", config_file.c_str());
    }
    
    if ((err = parse_buffer(buffer)) != srs_success) {
        return srs_error_wrap(err, "parse buffer");
    }
    
    return err;
}

srs_error_t SrsConfig::build_buffer(string src, SrsConfigBuffer** pbuffer)
{
    srs_error_t err = srs_success;

    SrsConfigBuffer* buffer = new SrsConfigBuffer();

    if ((err = buffer->fullfill(src.c_str())) != srs_success) {
        srs_freep(buffer);
        return srs_error_wrap(err, "read from src %s", src.c_str());
    }

    *pbuffer = buffer;
    return err;
}
// LCOV_EXCL_STOP

srs_error_t SrsConfig::check_config()
{
    srs_error_t err = srs_success;
    
    if ((err = check_normal_config()) != srs_success) {
        return srs_error_wrap(err, "check normal");
    }
    
    if ((err = check_number_connections()) != srs_success) {
        return srs_error_wrap(err, "check connections");
    }

    // If use the full.conf, fail.
    if (is_full_config()) {
        return srs_error_new(ERROR_SYSTEM_CONFIG_INVALID,
            "never use full.conf(%s)", config_file.c_str());
    }
    
    return err;
}

srs_error_t SrsConfig::check_normal_config()
{
    srs_error_t err = srs_success;
    
    printf("srs checking config...\n");
    
    ////////////////////////////////////////////////////////////////////////
    // check empty
    ////////////////////////////////////////////////////////////////////////
    if (!env_only_ && root->directives.size() == 0) {
        return srs_error_new(ERROR_SYSTEM_CONFIG_INVALID, "conf is empty");
    }
    
    ////////////////////////////////////////////////////////////////////////
    // check root directives.
    ////////////////////////////////////////////////////////////////////////
    for (int i = 0; i < (int)root->directives.size(); i++) {
        SrsConfDirective* conf = root->at(i);
        std::string n = conf->name;
        if (n != "listen" && n != "pid" && n != "chunk_size" && n != "ff_log_dir"
            && n != "srs_log_tank" && n != "srs_log_level" && n != "srs_log_level_v2" && n != "srs_log_file"
            && n != "max_connections" && n != "daemon" && n != "heartbeat" && n != "tencentcloud_apm"
            && n != "http_api" && n != "stats" && n != "vhost" && n != "pithy_print_ms"
            && n != "http_server" && n != "stream_caster" && n != "rtc_server" && n != "srt_server"
            && n != "utc_time" && n != "work_dir" && n != "asprocess" && n != "server_id"
            && n != "ff_log_level" && n != "grace_final_wait" && n != "force_grace_quit"
            && n != "grace_start_wait" && n != "empty_ip_ok" && n != "disable_daemon_for_docker"
            && n != "inotify_auto_reload" && n != "auto_reload_for_docker" && n != "tcmalloc_release_rate"
            && n != "query_latest_version" && n != "first_wait_for_qlv" && n != "threads"
            && n != "circuit_breaker" && n != "is_full" && n != "in_docker" && n != "tencentcloud_cls"
            && n != "exporter"
            ) {
            return srs_error_new(ERROR_SYSTEM_CONFIG_INVALID, "illegal directive %s", n.c_str());
        }
    }
    if (true) {
        SrsConfDirective* conf = root->get("http_api");
        for (int i = 0; conf && i < (int)conf->directives.size(); i++) {
            SrsConfDirective* obj = conf->at(i);
            string n = obj->name;
            if (n != "enabled" && n != "listen" && n != "crossdomain" && n != "raw_api" && n != "auth" && n != "https") {
                return srs_error_new(ERROR_SYSTEM_CONFIG_INVALID, "illegal http_api.%s", n.c_str());
            }
            
            if (n == "raw_api") {
                for (int j = 0; j < (int)obj->directives.size(); j++) {
                    string m = obj->at(j)->name;
                    if (m != "enabled" && m != "allow_reload" && m != "allow_query" && m != "allow_update") {
                        return srs_error_new(ERROR_SYSTEM_CONFIG_INVALID, "illegal http_api.raw_api.%s", m.c_str());
                    }
                }
            }

            if (n == "auth") {
                for (int j = 0; j < (int)obj->directives.size(); j++) {
                    string m = obj->at(j)->name;
                    if (m != "enabled" && m != "username" && m != "password") {
                        return srs_error_new(ERROR_SYSTEM_CONFIG_INVALID, "illegal http_api.auth.%s", m.c_str());
                    }
                }
            }
        }
    }
    if (true) {
        SrsConfDirective* conf = root->get("http_server");
        for (int i = 0; conf && i < (int)conf->directives.size(); i++) {
            string n = conf->at(i)->name;
            if (n != "enabled" && n != "listen" && n != "dir" && n != "crossdomain" && n != "https") {
                return srs_error_new(ERROR_SYSTEM_CONFIG_INVALID, "illegal http_stream.%s", n.c_str());
            }
        }
    }
    if (true) {
        SrsConfDirective* conf = root->get("srt_server");
        for (int i = 0; conf && i < (int)conf->directives.size(); i++) {
            string n = conf->at(i)->name;
            if (n != "enabled" && n != "listen" && n != "maxbw"
                && n != "mss" && n != "latency" && n != "recvlatency"
                && n != "peerlatency" && n != "connect_timeout"
                && n != "sendbuf" && n != "recvbuf" && n != "payloadsize"
                && n != "default_app" && n != "sei_filter" && n != "mix_correct"
                && n != "tlpktdrop" && n != "tsbpdmode" && n != "passphrase" && n != "pbkeylen") {
                return srs_error_new(ERROR_SYSTEM_CONFIG_INVALID, "illegal srt_server.%s", n.c_str());
            }
        }
    }

    if (true) {
        SrsConfDirective* conf = root->get("rtc_server");
        for (int i = 0; conf && i < (int)conf->directives.size(); i++) {
            string n = conf->at(i)->name;
            if (n != "enabled" && n != "listen" && n != "dir" && n != "candidate" && n != "ecdsa" && n != "tcp"
                && n != "encrypt" && n != "reuseport" && n != "merge_nalus" && n != "black_hole" && n != "protocol"
                && n != "ip_family" && n != "api_as_candidates" && n != "resolve_api_domain"
                && n != "keep_api_domain" && n != "use_auto_detect_network_ip") {
                return srs_error_new(ERROR_SYSTEM_CONFIG_INVALID, "illegal rtc_server.%s", n.c_str());
            }
        }
    }
    if (true) {
        SrsConfDirective* conf = root->get("exporter");
        for (int i = 0; conf && i < (int)conf->directives.size(); i++) {
            string n = conf->at(i)->name;
            if (n != "enabled" && n != "listen" && n != "label" && n != "tag") {
                return srs_error_new(ERROR_SYSTEM_CONFIG_INVALID, "illegal exporter.%s", n.c_str());
            }
        }
    }

    ////////////////////////////////////////////////////////////////////////
    // check listen for rtmp.
    ////////////////////////////////////////////////////////////////////////
    if (true) {
        vector<string> listens = get_listens();
        if (!env_only_ && listens.size() <= 0) {
            return srs_error_new(ERROR_SYSTEM_CONFIG_INVALID, "listen requires params");
        }
        for (int i = 0; i < (int)listens.size(); i++) {
            int port; string ip;
            srs_parse_endpoint(listens[i], ip, port);

            // check ip
            if (!srs_check_ip_addr_valid(ip)) {
                return srs_error_new(ERROR_SYSTEM_CONFIG_INVALID, "listen.ip=%s is invalid", ip.c_str());
            }

            // check port
            if (port <= 0) {
                return srs_error_new(ERROR_SYSTEM_CONFIG_INVALID, "listen.port=%d is invalid", port);
            }
        }
    }
    
    ////////////////////////////////////////////////////////////////////////
    // check log name and level
    ////////////////////////////////////////////////////////////////////////
    if (true) {
        std::string log_filename = this->get_log_file();
        if (get_log_tank_file() && log_filename.empty()) {
            return srs_error_new(ERROR_SYSTEM_CONFIG_INVALID, "log file is empty");
        }
        if (get_log_tank_file()) {
            printf("you can check log by: tail -n 30 -f %s\n", log_filename.c_str());
            printf("please check SRS by: ./etc/init.d/srs status\n");
        } else {
            printf("write log to console");
        }
    }

    // asprocess conflict with daemon
    if (get_asprocess() && get_daemon()) {
        return srs_error_new(ERROR_SYSTEM_CONFIG_INVALID, "daemon conflicts with asprocess");
    }
    
    return err;
}

// LCOV_EXCL_START
srs_error_t SrsConfig::check_number_connections()
{
    srs_error_t err = srs_success;
    
    ////////////////////////////////////////////////////////////////////////
    // check max connections
    ////////////////////////////////////////////////////////////////////////
    int nb_connections = get_max_connections();
    if (nb_connections <= 0) {
        return srs_error_new(ERROR_SYSTEM_CONFIG_INVALID, "max_connections=%d is invalid", nb_connections);
    }

    // check max connections of system limits
    int nb_total = nb_connections + 128; // Simple reserved some fds.
    int max_open_files = (int)sysconf(_SC_OPEN_MAX);
    if (nb_total >= max_open_files) {
        printf("max_connections=%d, system limit to %d, please run: ulimit -HSn %d", nb_connections, max_open_files, srs_max(10000, nb_connections * 10));
        return srs_error_new(ERROR_SYSTEM_CONFIG_INVALID, "%d exceed max open files=%d", nb_total, max_open_files);
    }

    return err;
}
// LCOV_EXCL_STOP

srs_error_t SrsConfig::parse_buffer(SrsConfigBuffer* buffer)
{
    srs_error_t err = srs_success;

    // We use a new root to parse buffer, to allow parse multiple times.
    srs_freep(root);
    root = new SrsConfDirective();

    // Parse root tree from buffer.
    if ((err = root->parse(buffer, this)) != srs_success) {
        return srs_error_wrap(err, "root parse");
    }
    
    return err;
}

string SrsConfig::cwd()
{
    return _cwd;
}

string SrsConfig::argv()
{
    return _argv;
}

bool SrsConfig::get_daemon()
{
    SRS_OVERWRITE_BY_ENV_BOOL2("srs.daemon");

    SrsConfDirective* conf = root->get("daemon");
    if (!conf || conf->arg0().empty()) {
        return true;
    }
    
    return SRS_CONF_PERFER_TRUE(conf->arg0());
}

bool SrsConfig::get_in_docker()
{
    SRS_OVERWRITE_BY_ENV_BOOL("srs.in_docker"); // SRS_IN_DOCKER

    static bool DEFAULT = false;

    SrsConfDirective* conf = root->get("in_docker");
    if (!conf) {
        return DEFAULT;
    }

    return SRS_CONF_PERFER_FALSE(conf->arg0());
}

bool SrsConfig::is_full_config()
{
    static bool DEFAULT = false;

    SrsConfDirective* conf = root->get("is_full");
    if (!conf) {
        return DEFAULT;
    }

    return SRS_CONF_PERFER_FALSE(conf->arg0());
}

SrsConfDirective* SrsConfig::get_root()
{
    return root;
}

string srs_server_id_path(string pid_file)
{
    string path = srs_string_replace(pid_file, ".pid", ".id");
    if (!srs_string_ends_with(path, ".id")) {
        path += ".id";
    }
    return path;
}

string srs_try_read_file(string path) {
    srs_error_t err = srs_success;

    SrsFileReader r;
    if ((err = r.open(path)) != srs_success) {
        srs_freep(err);
        return "";
    }

    static char buf[1024];
    ssize_t nn = 0;
    if ((err = r.read(buf, sizeof(buf), &nn)) != srs_success) {
        srs_freep(err);
        return "";
    }

    if (nn > 0) {
        return string(buf, nn);
    }
    return "";
}

void srs_try_write_file(string path, string content) {
    srs_error_t err = srs_success;

    SrsFileWriter w;
    if ((err = w.open(path)) != srs_success) {
        srs_freep(err);
        return;
    }

    if ((err = w.write((void*)content.data(), content.length(), NULL)) != srs_success) {
        srs_freep(err);
        return;
    }
}

string SrsConfig::get_server_id()
{
    static string DEFAULT = "";

    // Try to read DEFAULT from server id file.
    if (DEFAULT.empty()) {
        DEFAULT = srs_try_read_file(srs_server_id_path(get_pid_file()));
    }

    // Generate a random one if empty.
    if (DEFAULT.empty()) {
        DEFAULT = /*srs_generate_stat_vid()*/"1234567";
    }

    // Get the server id from env, config or DEFAULT.
    string server_id;

    if (!srs_getenv("srs.server_id").empty()) { // SRS_SERVER_ID
        server_id = srs_getenv("srs.server_id");
    } else {
        SrsConfDirective* conf = root->get("server_id");
        if (conf) {
            server_id = conf->arg0();
        }
    }

    if (server_id.empty()) {
        server_id = DEFAULT;
    }

    // Write server id to tmp file.
    srs_try_write_file(srs_server_id_path(get_pid_file()), server_id);

    return server_id;
}

int SrsConfig::get_max_connections()
{
    SRS_OVERWRITE_BY_ENV_INT("srs.max_connections"); // SRS_MAX_CONNECTIONS

    static int DEFAULT = 1000;
    
    SrsConfDirective* conf = root->get("max_connections");
    if (!conf || conf->arg0().empty()) {
        return DEFAULT;
    }
    
    return ::atoi(conf->arg0().c_str());
}

vector<string> SrsConfig::get_listens()
{
    std::vector<string> ports;

    if (!srs_getenv("srs.listen").empty()) { // SRS_LISTEN
        return srs_string_split(srs_getenv("srs.listen"), " ");
    }
    
    SrsConfDirective* conf = root->get("listen");
    if (!conf) {
        return ports;
    }
    
    for (int i = 0; i < (int)conf->args.size(); i++) {
        ports.push_back(conf->args.at(i));
    }
    
    return ports;
}

string SrsConfig::get_pid_file()
{
    SRS_OVERWRITE_BY_ENV_STRING("srs.pid"); // SRS_PID

    static string DEFAULT = "./objs/srs.pid";
    
    SrsConfDirective* conf = root->get("pid");
    
    if (!conf || conf->arg0().empty()) {
        return DEFAULT;
    }
    
    return conf->arg0();
}

srs_utime_t SrsConfig::get_pithy_print()
{
    SRS_OVERWRITE_BY_ENV_MILLISECONDS("srs.pithy_print_ms"); // SRS_PITHY_PRINT_MS

    static srs_utime_t DEFAULT = 10 * SRS_UTIME_SECONDS;
    
    SrsConfDirective* conf = root->get("pithy_print_ms");
    if (!conf || conf->arg0().empty()) {
        return DEFAULT;
    }
    
    return (srs_utime_t)(::atoi(conf->arg0().c_str()) * SRS_UTIME_MILLISECONDS);
}

bool SrsConfig::get_utc_time()
{
    SRS_OVERWRITE_BY_ENV_BOOL("srs.utc_time"); // SRS_UTC_TIME

    static bool DEFAULT = false;
    
    SrsConfDirective* conf = root->get("utc_time");
    if (!conf || conf->arg0().empty()) {
        return DEFAULT;
    }
    
    return SRS_CONF_PERFER_FALSE(conf->arg0());
}

string SrsConfig::get_work_dir()
{
    SRS_OVERWRITE_BY_ENV_STRING("srs.work_dir"); // SRS_WORK_DIR

    static string DEFAULT = "./";
    
    SrsConfDirective* conf = root->get("work_dir");
    if( !conf || conf->arg0().empty()) {
        return DEFAULT;
    }
    
    return conf->arg0();
}

bool SrsConfig::get_asprocess()
{
    SRS_OVERWRITE_BY_ENV_BOOL("srs.asprocess"); // SRS_ASPROCESS

    static bool DEFAULT = false;
    
    SrsConfDirective* conf = root->get("asprocess");
    if (!conf || conf->arg0().empty()) {
        return DEFAULT;
    }
    
    return SRS_CONF_PERFER_FALSE(conf->arg0());
}

bool SrsConfig::whether_query_latest_version()
{
    SRS_OVERWRITE_BY_ENV_BOOL2("srs.query_latest_version"); // SRS_QUERY_LATEST_VERSION

    static bool DEFAULT = true;

    SrsConfDirective* conf = root->get("query_latest_version");
    if (!conf) {
        return DEFAULT;
    }

    return SRS_CONF_PERFER_TRUE(conf->arg0());
}

bool SrsConfig::get_log_tank_file()
{
    if (!srs_getenv("srs.srs_log_tank").empty()) { // SRS_SRS_LOG_TANK
        return srs_getenv("srs.srs_log_tank") != "console";
    }
    if (!srs_getenv("srs.log_tank").empty()) { // SRS_LOG_TANK
        return srs_getenv("srs.log_tank") != "console";
    }

    static bool DEFAULT = true;
    
    SrsConfDirective* conf = root->get("srs_log_tank");
    if (!conf || conf->arg0().empty()) {
        return DEFAULT;
    }
    
    return conf->arg0() != "console";
}

string SrsConfig::get_log_level()
{
    SRS_OVERWRITE_BY_ENV_STRING("srs.srs_log_level"); // SRS_SRS_LOG_LEVEL
    SRS_OVERWRITE_BY_ENV_STRING("srs.log_level"); // SRS_LOG_LEVEL

    static string DEFAULT = "trace";

    SrsConfDirective* conf = root->get("srs_log_level");
    if (!conf || conf->arg0().empty()) {
        return DEFAULT;
    }

    return conf->arg0();
}

string SrsConfig::get_log_level_v2()
{
    SRS_OVERWRITE_BY_ENV_STRING("srs.srs_log_level_v2"); // SRS_SRS_LOG_LEVEL_V2
    SRS_OVERWRITE_BY_ENV_STRING("srs.log_level_v2"); // SRS_LOG_LEVEL_V2

    static string DEFAULT = "";

    SrsConfDirective* conf = root->get("srs_log_level_v2");
    if (!conf || conf->arg0().empty()) {
        return DEFAULT;
    }

    return conf->arg0();
}

string SrsConfig::get_log_file()
{
    SRS_OVERWRITE_BY_ENV_STRING("srs.srs_log_file"); // SRS_SRS_LOG_FILE
    SRS_OVERWRITE_BY_ENV_STRING("srs.log_file"); // SRS_LOG_FILE

    static string DEFAULT = "./objs/srs.log";
    
    SrsConfDirective* conf = root->get("srs_log_file");
    if (!conf || conf->arg0().empty()) {
        return DEFAULT;
    }
    
    return conf->arg0();
}

bool SrsConfig::get_ff_log_enabled()
{
    string log = get_ff_log_dir();
    return log != SRS_CONSTS_NULL_FILE;
}

string SrsConfig::get_ff_log_dir()
{
    SRS_OVERWRITE_BY_ENV_STRING("srs.ff_log_dir"); // SRS_FF_LOG_DIR

    static string DEFAULT = "./objs";
    
    SrsConfDirective* conf = root->get("ff_log_dir");
    if (!conf || conf->arg0().empty()) {
        return DEFAULT;
    }
    
    return conf->arg0();
}

string SrsConfig::get_ff_log_level()
{
    SRS_OVERWRITE_BY_ENV_STRING("srs.ff_log_level"); // SRS_FF_LOG_LEVEL

    static string DEFAULT = "info";

    SrsConfDirective* conf = root->get("ff_log_level");
    if (!conf || conf->arg0().empty()) {
        return DEFAULT;
    }

    return conf->arg0();
}

string SrsConfig::get_http_server_listen()
{
    static string DEFAULT = "9090";//找不到配置项时使用默认值

    SrsConfDirective* conf = root->get("http_server");//先从根指令获取子指令
    for (int i = 0; conf && i < (int)conf->directives.size(); i++) {
        //再遍历子指令里面的子指令
        string n = conf->at(i)->name;
        if(n == "listen" && !conf->at(i)->arg0().empty())
        {
            //如果这一级子指令名称符合，返回子指令的第一个参数
            printf("http_server,listen=%s\n",conf->at(i)->arg0().c_str());
            return conf->at(i)->arg0();
        }
    }

    return DEFAULT;
}


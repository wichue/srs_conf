#ifndef CHW_ADAPT_H
#define CHW_ADAPT_H

#include <string>
#include <stdarg.h>
#include <inttypes.h>
#include <stdint.h>
#include <sys/stat.h>
typedef int64_t srs_utime_t;
// The time unit in ms, for example 100 * SRS_UTIME_MILLISECONDS means 100ms.
#define SRS_UTIME_MILLISECONDS 1000

// Convert srs_utime_t as ms.
#define srsu2ms(us) ((us) / SRS_UTIME_MILLISECONDS)
#define srsu2msi(us) int((us) / SRS_UTIME_MILLISECONDS)

// Convert srs_utime_t as second.
#define srsu2s(us) ((us) / SRS_UTIME_SECONDS)
#define srsu2si(us) ((us) / SRS_UTIME_SECONDS)

// Them time duration = end - start. return 0, if start or end is 0.
srs_utime_t srs_duration(srs_utime_t start, srs_utime_t end);

// The time unit in ms, for example 120 * SRS_UTIME_SECONDS means 120s.
#define SRS_UTIME_SECONDS 1000000LL

// The time unit in minutes, for example 3 * SRS_UTIME_MINUTES means 3m.
#define SRS_UTIME_MINUTES 60000000LL

// The time unit in hours, for example 2 * SRS_UTIME_HOURS means 2h.
#define SRS_UTIME_HOURS 3600000000LL

#define SRS_PERF_MERGED_READ
// the default config of mr.
#define SRS_PERF_MR_ENABLED false
#define SRS_PERF_MR_SLEEP (350 * SRS_UTIME_MILLISECONDS)

// For tcmalloc, set the default release rate.
// @see https://gperftools.github.io/gperftools/tcmalloc.html
#define SRS_PERF_TCMALLOC_RELEASE_RATE 0.8
#define SRS_CONSTS_NULL_FILE "/dev/null"
#define SRS_CONSTS_LOCALHOST "127.0.0.1"
#define SRS_CONSTS_LOCALHOST_NAME "localhost"

#define srs_freep(p) \
    delete p; \
    p = NULL; \
    (void)0
// Please use the freepa(T[]) to free an array, otherwise the behavior is undefined.
#define srs_freepa(pa) \
    delete[] pa; \
    pa = NULL; \
    (void)0

// CR             = <US-ASCII CR, carriage return (13)>
#define SRS_CONSTS_CR '\r' // 0x0D
// LF             = <US-ASCII LF, linefeed (10)>
#define SRS_CONSTS_LF '\n' // 0x0A
// SP             = <US-ASCII SP, space>
#define SRS_CONSTS_SP ' ' // 0x20
// SE             = <US-ASCII SE, semicolon>
#define SRS_CONSTS_SE ';' // 0x3b
// LB             = <US-ASCII SE, left-brace>
#define SRS_CONSTS_LB '{' // 0x7b
// RB             = <US-ASCII SE, right-brace>
#define SRS_CONSTS_RB '}' // 0x7d

#define VERSION_MAJOR       5
#define VERSION_MINOR       0
#define VERSION_REVISION    168

#define SRS_INTERNAL_STR(v) #v
#define SRS_XSTR(v) SRS_INTERNAL_STR(v)

#define SRS_CONSTS_RTMP_MIN_CHUNK_SIZE 128
#define SRS_CONSTS_RTMP_MAX_CHUNK_SIZE 65536

#define RTMP_SIG_SRS_KEY "SRS"
#define RTMP_SIG_SRS_CODE "Bee"
#define RTMP_SIG_SRS_URL "https://github.com/ossrs/srs"
#define RTMP_SIG_SRS_LICENSE "MIT"
#define SRS_CONSTRIBUTORS "https://github.com/ossrs/srs/blob/develop/trunk/AUTHORS.md#contributors"
#define RTMP_SIG_SRS_VERSION SRS_XSTR(VERSION_MAJOR) "." SRS_XSTR(VERSION_MINOR) "." SRS_XSTR(VERSION_REVISION)
#define RTMP_SIG_SRS_SERVER RTMP_SIG_SRS_KEY "/" RTMP_SIG_SRS_VERSION "(" RTMP_SIG_SRS_CODE ")"
#define RTMP_SIG_SRS_DOMAIN "ossrs.net"
#define RTMP_SIG_SRS_AUTHORS    "dev"
#define SRS_CONSTS_LOOPBACK "0.0.0.0"
#define SRS_CONSTS_LOOPBACK6 "::"
using namespace std;
#include <sys/socket.h>
#include <sys/unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <vector>
#include "ifaddrs.h"
#include <iostream>
#include <iosfwd>
#include <sstream>
#include <net/if.h>
#include <netdb.h>
#include <string.h>
#include <map>

bool srs_string_ends_with(string str, string flag);
// Get local ip, fill to @param ips
struct SrsIPAddress
{
    // The network interface name, such as eth0, en0, eth1.
    std::string ifname;
    // The IP v4 or v6 address.
    std::string ip;
    // Whether the ip is IPv4 address.
    bool is_ipv4;
    // Whether the ip is internet public IP address.
    bool is_internet;
    // Whether the ip is loopback, such as 127.0.0.1
    bool is_loopback;
};
extern std::vector<SrsIPAddress*>& srs_get_local_ips();
extern vector<SrsIPAddress*> _srs_system_ips;
void srs_free_global_system_ips();

// we detect all network device as internet or intranet device, by its ip address.
//      key is device name, for instance, eth0
//      value is whether internet, for instance, true.
static std::map<std::string, bool> _srs_device_ifs;
bool srs_net_device_is_internet(const sockaddr* addr);

void discover_network_iface(ifaddrs* cur, vector<SrsIPAddress*>& ips, stringstream& ss0, stringstream& ss1, bool ipv6, bool loopback);

void retrieve_local_ips();

vector<SrsIPAddress*>& srs_get_local_ips();

bool srs_check_ip_addr_valid(string ip);

string srs_any_address_for_listener();

void srs_parse_endpoint(string hostport, string& ip, int& port);

std::string srs_fmt(const char* fmt, ...);

string srs_int2str(int64_t value);

// 自动释放对象
// To delete object.
#define SrsAutoFree(className, instance) \
    impl_SrsAutoFree<className> _auto_free_##instance(&instance, false, false, NULL)
// To delete array.
#define SrsAutoFreeA(className, instance) \
    impl_SrsAutoFree<className> _auto_free_array_##instance(&instance, true, false, NULL)
// Use free instead of delete.
#define SrsAutoFreeF(className, instance) \
    impl_SrsAutoFree<className> _auto_free_##instance(&instance, false, true, NULL)
// Use hook instead of delete.
#define SrsAutoFreeH(className, instance, hook) \
    impl_SrsAutoFree<className> _auto_free_##instance(&instance, false, false, hook)
// The template implementation.
template<class T>
class impl_SrsAutoFree
{
private:
    T** ptr;
    bool is_array;
    bool _use_free;
    void (*_hook)(T*);
public:
    // If use_free, use free(void*) to release the p.
    // If specified hook, use hook(p) to release it.
    // Use delete to release p, or delete[] if p is an array.
    impl_SrsAutoFree(T** p, bool array, bool use_free, void (*hook)(T*)) {
        ptr = p;
        is_array = array;
        _use_free = use_free;
        _hook = hook;
    }

    virtual ~impl_SrsAutoFree() {
        if (ptr == NULL || *ptr == NULL) {
            return;
        }

        if (_use_free) {
            free(*ptr);
        } else if (_hook) {
            _hook(*ptr);
        } else {
            if (is_array) {
                delete[] *ptr;
            } else {
                delete *ptr;
            }
        }

        *ptr = NULL;
    }
};

bool srs_path_exists(std::string path);

vector<string> srs_string_split(string s, string seperator);

string srs_string_replace(string str, string old_str, string new_str);

bool srs_string_starts_with(string str, string flag);
string srs_getenv(const string& key);


#endif // CHW_ADAPT_H

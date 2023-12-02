#include "chw_adapt.h"

vector<SrsIPAddress*> _srs_system_ips;
bool srs_string_ends_with(string str, string flag)
{
    const size_t pos = str.rfind(flag);
    return (pos != string::npos) && (pos == str.length() - flag.length());
}

extern std::vector<SrsIPAddress*>& srs_get_local_ips();

void srs_free_global_system_ips()
{
    vector<SrsIPAddress*>& ips = _srs_system_ips;

    // Release previous IPs.
    for (int i = 0; i < (int)ips.size(); i++) {
        SrsIPAddress* ip = ips[i];
        srs_freep(ip);
    }
    ips.clear();
}

// we detect all network device as internet or intranet device, by its ip address.
//      key is device name, for instance, eth0
//      value is whether internet, for instance, true.

bool srs_net_device_is_internet(const sockaddr* addr)
{
    if(addr->sa_family == AF_INET) {
        const in_addr inaddr = ((sockaddr_in*)addr)->sin_addr;
        const uint32_t addr_h = ntohl(inaddr.s_addr);

        // lo, 127.0.0.0-127.0.0.1
        if (addr_h >= 0x7f000000 && addr_h <= 0x7f000001) {
            return false;
        }

        // Class A 10.0.0.0-10.255.255.255
        if (addr_h >= 0x0a000000 && addr_h <= 0x0affffff) {
            return false;
        }

        // Class B 172.16.0.0-172.31.255.255
        if (addr_h >= 0xac100000 && addr_h <= 0xac1fffff) {
            return false;
        }

        // Class C 192.168.0.0-192.168.255.255
        if (addr_h >= 0xc0a80000 && addr_h <= 0xc0a8ffff) {
            return false;
        }
    } else if(addr->sa_family == AF_INET6) {
        const sockaddr_in6* a6 = (const sockaddr_in6*)addr;

        // IPv6 loopback is ::1
        if (IN6_IS_ADDR_LOOPBACK(&a6->sin6_addr)) {
            return false;
        }

        // IPv6 unspecified is ::
        if (IN6_IS_ADDR_UNSPECIFIED(&a6->sin6_addr)) {
            return false;
        }

        // From IPv4, you might know APIPA (Automatic Private IP Addressing) or AutoNet.
        // Whenever automatic IP configuration through DHCP fails.
        // The prefix of a site-local address is FE80::/10.
        if (IN6_IS_ADDR_LINKLOCAL(&a6->sin6_addr)) {
            return false;
        }

        // Site-local addresses are equivalent to private IP addresses in IPv4.
        // The prefix of a site-local address is FEC0::/10.
        // https://4sysops.com/archives/ipv6-tutorial-part-6-site-local-addresses-and-link-local-addresses/
        if (IN6_IS_ADDR_SITELOCAL(&a6->sin6_addr)) {
            return false;
        }

        // Others.
        if (IN6_IS_ADDR_MULTICAST(&a6->sin6_addr)) {
            return false;
        }
        if (IN6_IS_ADDR_MC_NODELOCAL(&a6->sin6_addr)) {
            return false;
        }
        if (IN6_IS_ADDR_MC_LINKLOCAL(&a6->sin6_addr)) {
            return false;
        }
        if (IN6_IS_ADDR_MC_SITELOCAL(&a6->sin6_addr)) {
            return false;
        }
        if (IN6_IS_ADDR_MC_ORGLOCAL(&a6->sin6_addr)) {
            return false;
        }
        if (IN6_IS_ADDR_MC_GLOBAL(&a6->sin6_addr)) {
            return false;
        }
    }

    return true;
}
void discover_network_iface(ifaddrs* cur, vector<SrsIPAddress*>& ips, stringstream& ss0, stringstream& ss1, bool ipv6, bool loopback)
{
    char saddr[64];
    char* h = (char*)saddr;
    socklen_t nbh = (socklen_t)sizeof(saddr);
    const int r0 = getnameinfo(cur->ifa_addr, sizeof(sockaddr_storage), h, nbh, NULL, 0, NI_NUMERICHOST);
    if(r0) {
        printf("convert local ip failed: %s", gai_strerror(r0));
        return;
    }

    std::string ip(saddr, strlen(saddr));
    ss0 << ", iface[" << (int)ips.size() << "] " << cur->ifa_name << " " << (ipv6? "ipv6":"ipv4")
        << " 0x" << std::hex << cur->ifa_flags  << std::dec << " " << ip;

    SrsIPAddress* ip_address = new SrsIPAddress();
    ip_address->ip = ip;
    ip_address->is_ipv4 = !ipv6;
    ip_address->is_loopback = loopback;
    ip_address->ifname = cur->ifa_name;
    ip_address->is_internet = srs_net_device_is_internet(cur->ifa_addr);
    ips.push_back(ip_address);

    // set the device internet status.
    if (!ip_address->is_internet) {
        ss1 << ", intranet ";
        _srs_device_ifs[cur->ifa_name] = false;
    } else {
        ss1 << ", internet ";
        _srs_device_ifs[cur->ifa_name] = true;
    }
    ss1 << cur->ifa_name << " " << ip;
}
void retrieve_local_ips()
{
    // Release previous IPs.
    srs_free_global_system_ips();

    vector<SrsIPAddress*>& ips = _srs_system_ips;

    // Get the addresses.
    ifaddrs* ifap;
    if (getifaddrs(&ifap) == -1) {
        printf("retrieve local ips, getifaddrs failed.");
        return;
    }

    stringstream ss0;
    ss0 << "ips";

    stringstream ss1;
    ss1 << "devices";

    // Discover IPv4 first.
    for (ifaddrs* p = ifap; p ; p = p->ifa_next) {
        ifaddrs* cur = p;

        // Ignore if no address for this interface.
        // @see https://github.com/ossrs/srs/issues/1087#issuecomment-408847115
        if (!cur->ifa_addr) {
            continue;
        }

        // retrieve IP address, ignore the tun0 network device, whose addr is NULL.
        // @see: https://github.com/ossrs/srs/issues/141
        bool ipv4 = (cur->ifa_addr->sa_family == AF_INET);
        bool ready = (cur->ifa_flags & IFF_UP) && (cur->ifa_flags & IFF_RUNNING);
        // Ignore IFF_PROMISC(Interface is in promiscuous mode), which may be set by Wireshark.
        bool ignored = (!cur->ifa_addr) || (cur->ifa_flags & IFF_LOOPBACK) || (cur->ifa_flags & IFF_POINTOPOINT);
        bool loopback = (cur->ifa_flags & IFF_LOOPBACK);
        if (ipv4 && ready && !ignored) {
            discover_network_iface(cur, ips, ss0, ss1, false, loopback);
        }
    }

    // Then, discover IPv6 addresses.
    for (ifaddrs* p = ifap; p ; p = p->ifa_next) {
        ifaddrs* cur = p;

        // Ignore if no address for this interface.
        // @see https://github.com/ossrs/srs/issues/1087#issuecomment-408847115
        if (!cur->ifa_addr) {
            continue;
        }

        // retrieve IP address, ignore the tun0 network device, whose addr is NULL.
        // @see: https://github.com/ossrs/srs/issues/141
        bool ipv6 = (cur->ifa_addr->sa_family == AF_INET6);
        bool ready = (cur->ifa_flags & IFF_UP) && (cur->ifa_flags & IFF_RUNNING);
        bool ignored = (!cur->ifa_addr) || (cur->ifa_flags & IFF_POINTOPOINT) || (cur->ifa_flags & IFF_PROMISC) || (cur->ifa_flags & IFF_LOOPBACK);
        bool loopback = (cur->ifa_flags & IFF_LOOPBACK);
        if (ipv6 && ready && !ignored) {
            discover_network_iface(cur, ips, ss0, ss1, true, loopback);
        }
    }

    // If empty, disover IPv4 loopback.
    if (ips.empty()) {
        for (ifaddrs* p = ifap; p ; p = p->ifa_next) {
            ifaddrs* cur = p;

            // Ignore if no address for this interface.
            // @see https://github.com/ossrs/srs/issues/1087#issuecomment-408847115
            if (!cur->ifa_addr) {
                continue;
            }

            // retrieve IP address, ignore the tun0 network device, whose addr is NULL.
            // @see: https://github.com/ossrs/srs/issues/141
            bool ipv4 = (cur->ifa_addr->sa_family == AF_INET);
            bool ready = (cur->ifa_flags & IFF_UP) && (cur->ifa_flags & IFF_RUNNING);
            bool ignored = (!cur->ifa_addr) || (cur->ifa_flags & IFF_POINTOPOINT) || (cur->ifa_flags & IFF_PROMISC);
            bool loopback = (cur->ifa_flags & IFF_LOOPBACK);
            if (ipv4 && ready && !ignored) {
                discover_network_iface(cur, ips, ss0, ss1, false, loopback);
            }
        }
    }

    printf("%s", ss0.str().c_str());
    printf("%s", ss1.str().c_str());

    freeifaddrs(ifap);
}
vector<SrsIPAddress*>& srs_get_local_ips()
{
    if (_srs_system_ips.empty()) {
        retrieve_local_ips();
    }

    return _srs_system_ips;
}

bool srs_check_ip_addr_valid(string ip)
{
    unsigned char buf[sizeof(struct in6_addr)];

    // check ipv4
    int ret = inet_pton(AF_INET, ip.data(), buf);
    if (ret > 0) {
        return true;
    }

    ret = inet_pton(AF_INET6, ip.data(), buf);
    if (ret > 0) {
        return true;
    }

    return false;
}

string srs_any_address_for_listener()
{
    bool ipv4_active = false;
    bool ipv6_active = false;

    if (true) {
        int fd = socket(AF_INET, SOCK_DGRAM, 0);
        if(fd != -1) {
            ipv4_active = true;
            close(fd);
        }
    }
    if (true) {
        int fd = socket(AF_INET6, SOCK_DGRAM, 0);
        if(fd != -1) {
            ipv6_active = true;
            close(fd);
        }
    }

    if (ipv6_active && !ipv4_active) {
        return SRS_CONSTS_LOOPBACK6;
    }
    return SRS_CONSTS_LOOPBACK;
}
void srs_parse_endpoint(string hostport, string& ip, int& port)
{
    const size_t pos = hostport.rfind(":");   // Look for ":" from the end, to work with IPv6.
    if (pos != std::string::npos) {
        if ((pos >= 1) && (hostport[0] == '[') && (hostport[pos - 1] == ']')) {
            // Handle IPv6 in RFC 2732 format, e.g. [3ffe:dead:beef::1]:1935
            ip = hostport.substr(1, pos - 2);
        } else {
            // Handle IP address
            ip = hostport.substr(0, pos);
        }

        const string sport = hostport.substr(pos + 1);
        port = ::atoi(sport.c_str());
    } else {
        ip = srs_any_address_for_listener();
        port = ::atoi(hostport.c_str());
    }
}

std::string srs_fmt(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    static char buf[8192];
    int r0 = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    string v;
    if (r0 > 0 && r0 < (int)sizeof(buf)) {
        v.append(buf, r0);
    }

    return v;
}

string srs_int2str(int64_t value)
{
    return srs_fmt("%" PRId64, value);
}


bool srs_path_exists(std::string path)
{
    struct stat st;

    // stat current dir, if exists, return error.
    if (stat(path.c_str(), &st) == 0) {
        return true;
    }

    return false;
}

vector<string> srs_string_split(string s, string seperator)
{
    vector<string> result;
    if(seperator.empty()){
        result.push_back(s);
        return result;
    }

    size_t posBegin = 0;
    size_t posSeperator = s.find(seperator);
    while (posSeperator != string::npos) {
        result.push_back(s.substr(posBegin, posSeperator - posBegin));
        posBegin = posSeperator + seperator.length(); // next byte of seperator
        posSeperator = s.find(seperator, posBegin);
    }
    // push the last element
    result.push_back(s.substr(posBegin));
    return result;
}

string srs_string_replace(string str, string old_str, string new_str)
{
    std::string ret = str;

    if (old_str == new_str) {
        return ret;
    }

    size_t pos = 0;
    while ((pos = ret.find(old_str, pos)) != std::string::npos) {
        ret = ret.replace(pos, old_str.length(), new_str);
        pos += new_str.length();
    }

    return ret;
}

bool srs_string_starts_with(string str, string flag)
{
    return str.find(flag) == 0;
}
string srs_getenv(const string& key)
{
    string ekey = key;
    if (srs_string_starts_with(key, "$")) {
        ekey = key.substr(1);
    }

    if (ekey.empty()) {
        return "";
    }

    std::string::iterator it;
    for (it = ekey.begin(); it != ekey.end(); ++it) {
        if (*it >= 'a' && *it <= 'z') {
            *it += ('A' - 'a');
        } else if (*it == '.') {
            *it = '_';
        }
    }

    char* value = ::getenv(ekey.c_str());
    if (value) {
        return value;
    }

    return "";
}

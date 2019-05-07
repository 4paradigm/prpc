#include <arpa/inet.h>
#include <net/if.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include <boost/regex.hpp>
#include <boost/format.hpp>

#include "common/include/common.h"
#include "common/include/defs.h"
#include "common/include/macro.h"
#include "common/include/pico_error_code.h"
#include "common/include/pico_log.h"

namespace paradigm4 {
namespace pico {

bool get_local_ip_by_hostname(std::string& ip_address) {
    int hostname_buf_len = 2048;

#ifdef __USE_POSIX
    hostname_buf_len = std::max(hostname_buf_len, HOST_NAME_MAX);
#endif

    char hostname_buf[hostname_buf_len];
    if (gethostname(hostname_buf, hostname_buf_len) != 0) {
        SLOG(WARNING) << "gethostname failed, errno: " << errno;
        return false;
    }

    hostent* host = gethostbyname(hostname_buf);
    if (host == nullptr) {
        SLOG(WARNING) << "gethostbyname failed, errno: " << h_errno
                      << ", hostname:" << hostname_buf;
        return false;
    }

    in_addr* address = (in_addr*)host->h_addr;
    if (address == nullptr) {
        SLOG(WARNING) << "h_addr is nullptr in host, hostname:" << hostname_buf;
        return false;
    }

    size_t ip_address_buf_len = 512u;
    char ip_address_buf[ip_address_buf_len];
    const char* ret = inet_ntop(host->h_addrtype, address, ip_address_buf, ip_address_buf_len);
    if (ret == nullptr) {
        SLOG(WARNING) << "inet_ntop failed, errno: " << errno << ", hostname:" << hostname_buf;
        return false;
    }
    ip_address = ip_address_buf;
    return true;
}

bool get_local_ip_by_ioctl(std::string& ip_address) {
    int sockfd = 0;
    char buf[512];
    struct ifconf ifconf;
    struct ifreq* ifreq;
    ifconf.ifc_len = 512;
    ifconf.ifc_buf = buf;
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        SLOG(WARNING) << "open socket failed. errno: " << errno;
        return false;
    }

    if (ioctl(sockfd, SIOCGIFCONF, &ifconf) < 0) {
        SLOG(WARNING) << "ioctl failed. errno: " << errno;
        return false;
    }

    if (close(sockfd) != 0) {
        SLOG(WARNING) << "close sockfd failed. errno: " << errno;
        return false;
    }

    ifreq = (struct ifreq*)buf;
    bool is_found = false;

    const size_t IP_BUF_LEN = 512u;
    char ip_buf[IP_BUF_LEN];
    for (int i = (ifconf.ifc_len / sizeof(struct ifreq)); i > 0; --i) {
        sockaddr_in* address = (sockaddr_in*)&(ifreq->ifr_addr);
        const char* ret
              = inet_ntop(address->sin_family, &(address->sin_addr), ip_buf, IP_BUF_LEN);
        if (ret == nullptr) {
            SLOG(WARNING) << "inet_ntop failed.";
            return false;
        }
        if (strncmp(ip_buf, "127.", 4) != 0) {
            ip_address = ip_buf;
            is_found = true;
            break;
        }
        ++ifreq;
    }
    return is_found;
}

bool guess_local_ip(std::string& name) {
    if (get_local_ip_by_hostname(name)) {
        return true;
    }

    RLOG(WARNING) << "Guess ip from hostname failed. Trying to guess from ioctl...";

    if (get_local_ip_by_ioctl(name)) {
        return true;
    }

    RLOG(WARNING) << "Sorry, guess local ip failed.";

    return false;
}

int find_cmdline_split_pos(int argc, char* argv[]) {
    if (argc <= 0 || argv == nullptr) {
        SLOG(WARNING) << "invalid input argc: " << argc << ", argv: " << (void*)argv;
        return -1;
    }

    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] != '-') {
            return i;
        }
    }
    SLOG(WARNING) << "no executable in cmd line options";
    return -1;
}

bool split_args(int* argc, char* argv[], std::vector<char*>* child_argv) {
    if (argc == nullptr || argv == nullptr || child_argv == nullptr) {
        SLOG(WARNING) << "invalid parameter nullptr argc: " << (void*)argc
                      << ", argv: " << (void*)argv << ", child_argv: " << (void*)child_argv;
        return false;
    }

    int pos = find_cmdline_split_pos(*argc, argv);
    if (pos <= 0 || pos >= *argc) {
        SLOG(WARNING) << "find cmdline wrapper/pico executable split position failed. pos: "
                      << pos << "(min: 1, max: " << (*argc - 1) << ")";
        return false;
    }

    child_argv->clear();
    for (int i = pos; i < *argc; ++i) {
        child_argv->push_back(argv[i]);
    }
    *argc = pos;
    return true;
}

bool ipv4_format_validator(const std::string& ip) {
    boost::regex ip_regex(REGEX_IPV4);

    bool check_ok = true;
    try {
        check_ok = regex_match(ip, ip_regex, boost::regex_constants::match_default);
    } catch (boost::regex_error& e) {
        SLOG(WARNING) << "boost regex exception: " << e.what();
        check_ok = false;
    } catch (std::exception& e) {
        SLOG(WARNING) << "std exception: " << e.what();
        check_ok = false;
    } catch (...) {
        SLOG(WARNING) << "unkown exception.";
        check_ok = false;
    }

    return check_ok;
}

bool is_local_ipv4_by_ioctl(const std::string& ip_address) {
    if (!ipv4_format_validator(ip_address)) {
        SLOG(WARNING) << "ip address format error: [ " << ip_address << " ]";
        return false;
    }

    int sockfd = 0;
    char buf[512];
    struct ifconf ifconf;
    struct ifreq* ifreq;
    ifconf.ifc_len = 512;
    ifconf.ifc_buf = buf;
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (sockfd < 0) {
        SLOG(WARNING) << "open socket failed. errno: " << errno;
        return false;
    }

    if (ioctl(sockfd, SIOCGIFCONF, &ifconf) < 0) {
        SLOG(WARNING) << "ioctl failed. errno: " << errno;
        return false;
    }

    if (close(sockfd) != 0) {
        SLOG(WARNING) << "close sockfd failed. errno: " << errno;
        return false;
    }

    ifreq = (struct ifreq*)buf;
    bool is_found = false;

    const size_t IP_BUF_LEN = 512u;
    char ip_buf[IP_BUF_LEN];
    for (int i = (ifconf.ifc_len / sizeof(struct ifreq)); i > 0; --i) {
        sockaddr_in* address = (sockaddr_in*)&(ifreq->ifr_addr);
        const char* ret
              = inet_ntop(address->sin_family, &(address->sin_addr), ip_buf, IP_BUF_LEN);
        if (ret == nullptr) {
            SLOG(WARNING) << "inet_ntop failed.";
            return false;
        }
        if (strncmp(ip_buf, ip_address.c_str(), IP_BUF_LEN) == 0) {
            is_found = true;
            break;
        }
        ++ifreq;
    }
    return is_found;
}

bool fetch_ip(const std::string& user_set_ip, std::string* ip) {
    if (ip == nullptr) {
        SLOG(WARNING) << "invalid parameter, cannot be nullptr";
        return false;
    }

    if (user_set_ip == "") {
        if (!guess_local_ip(*ip)) {
            RLOG(WARNING) << "guess local ip failed. user may configure it";
            return false;
        }
    } else {
        size_t p = user_set_ip.find(':');
        if (p == std::string::npos) {
            *ip = user_set_ip;
        } else {
            *ip = user_set_ip.substr(0, p);
        }
    }

    if (!ipv4_format_validator(*ip)) {
        RLOG(WARNING) << "ip format error: [ " << *ip << " ]" << user_set_ip;
        return false;
    }

    if (!is_local_ipv4_by_ioctl(*ip)) {
        RLOG(WARNING) << "ip is not local ip: [ " << *ip << " ]" << user_set_ip;
        return false;
    }
    return true;
}

} // namespace pico
} // namespace paradigm4

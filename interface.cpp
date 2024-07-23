#include <iostream>
#include <fstream>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <string>
#include <cstring>
#include <csignal>
#include <cerrno>

#define SOCKET_PATH "/tmp/a1.socket"
#define PATH_BASE "/sys/class/net/%s/"
#define BUFFER_SIZE 256
#define PATH_SIZE 256

using namespace std;

bool online = true;

int setIntfFlags(const string& intfName, short flag)
{
    int flagStat;
    int intfFd = socket(AF_INET, SOCK_DGRAM, 0);
    if (intfFd < 0)
    {
        cerr << "ERROR: socket isnt working" << endl;
        return 1;
    }

    struct ifreq intfReq;
    strncpy(intfReq.ifr_name, intfName.c_str(), IFNAMSIZ);
    intfReq.ifr_flags = flag;

    flagStat = ioctl(intfFd, SIOCSIFFLAGS, &intfReq);
    if (flagStat < 0)
    {
        cerr << "ERROR: ioctl response isnt working" << endl;
    }
    close(intfFd);
    return flagStat;
}

int turnIntfOn(const string& intfName)
{
    return setIntfFlags(intfName, IFF_UP | IFF_RUNNING);
}

void signalHandler(int sig) 
{

    online = false;
}

void interfaceInfo(const string& intf, string& stats) {
    string operstate;
    char statsPath[PATH_SIZE];
    int upCount = 0, downCount = 0, rxBytes = 0, rxDropped = 0,
        rxErrors = 0, rxPackets = 0, txBytes = 0, txDropped = 0,
        txErrors = 0, txPackets = 0;

    ifstream input;

    snprintf(statsPath, sizeof(statsPath), PATH_BASE "operstate", intf.c_str());
    input.open(statsPath);
    if (input.is_open()) {
        input >> operstate;
        input.close();
    }

    snprintf(statsPath, sizeof(statsPath), PATH_BASE "carrier_up_count", intf.c_str());
    input.open(statsPath);
    if (input.is_open()) {
        input >> upCount;
        input.close();
    }

    snprintf(statsPath, sizeof(statsPath), PATH_BASE "carrier_down_count", intf.c_str());
    input.open(statsPath);
    if (input.is_open()) {
        input >> downCount;
        input.close();
    }

    snprintf(statsPath, sizeof(statsPath), PATH_BASE "statistics/rx_bytes", intf.c_str());
    input.open(statsPath);
    if (input.is_open()) {
        input >> rxBytes;
        input.close();
    }

    snprintf(statsPath, sizeof(statsPath), PATH_BASE "statistics/rx_dropped", intf.c_str());
    input.open(statsPath);
    if (input.is_open()) {
        input >> rxDropped;
        input.close();
    }

    snprintf(statsPath, sizeof(statsPath), PATH_BASE "statistics/rx_errors", intf.c_str());
    input.open(statsPath);
    if (input.is_open()) {
        input >> rxErrors;
        input.close();
    }

    snprintf(statsPath, sizeof(statsPath), PATH_BASE "statistics/rx_packets", intf.c_str());
    input.open(statsPath);
    if (input.is_open()) {
        input >> rxPackets;
        input.close();
    }

    snprintf(statsPath, sizeof(statsPath), PATH_BASE "statistics/tx_bytes", intf.c_str());
    input.open(statsPath);
    if (input.is_open()) {
        input >> txBytes;
        input.close();
    }

    snprintf(statsPath, sizeof(statsPath), PATH_BASE "statistics/tx_dropped", intf.c_str());
    input.open(statsPath);
    if (input.is_open()) {
        input >> txDropped;
        input.close();
    }

    snprintf(statsPath, sizeof(statsPath), PATH_BASE "statistics/tx_errors", intf.c_str());
    input.open(statsPath);
    if (input.is_open()) {
        input >> txErrors;
        input.close();
    }

    snprintf(statsPath, sizeof(statsPath), PATH_BASE "statistics/tx_packets", intf.c_str());
    input.open(statsPath);
    if (input.is_open()) {
        input >> txPackets;
        input.close();
    }

    stats = "Interface:" + intf + " State:" + operstate + " upCount:" + std::to_string(upCount) + " downCount:" 
            + std::to_string(downCount) + "\n" + " rxBytes:" + std::to_string(rxBytes) + " rxDropped:" + std::to_string(rxDropped) + 
            " rxErrors:" + std::to_string(rxErrors) + " rxPackets:" + std::to_string(rxPackets) + "\n" + " txBytes:" + std::to_string(txBytes) + 
            " txDropped:" + std::to_string(txDropped) + " txErrors:" + std::to_string(txErrors) + " txPackets:" + std::to_string(txPackets) + "\n";
}

void runInterfaceMonitor(const string *intf) {
    struct sockaddr_un address;
    char buff[BUFFER_SIZE];

    int intfFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (intfFd < 0) {
        cerr << "ERROR: socket isnt working" << endl;
        exit(-1);
    }

    memset(&address, 0, sizeof(struct sockaddr_un));
    address.sun_family = AF_UNIX;
    strncpy(address.sun_path, SOCKET_PATH, sizeof(address.sun_path) - 1);

    while (connect(intfFd, (struct sockaddr*)&address, sizeof(struct sockaddr_un)) == -1) {
        if (errno == ENOENT) {
            sleep(1);
        } else {
            cerr << "ERROR: connection doesnt work" << endl;
            exit(-1);
        }
    }

    write(intfFd, "Ready", 6);

    while (online) {
        int bytesRead = read(intfFd, buff, BUFFER_SIZE);

        if (bytesRead <= 0) {
            if (bytesRead < 0) {
                cerr << "ERROR: cant read fd" << endl;
            }
            break;
        }

        buff[bytesRead] = '\0';
        cout << buff << endl;

        if (strncmp(buff, "Monitor", 7) == 0) {
            while (online) {
                string stats;
                interfaceInfo(*intf, stats);

                cout << stats << endl;

                if (stats.find("State:down") != string::npos) {
                    write(intfFd, "Link Down", 10);
                    turnIntfOn(*intf);
                }

                write(intfFd, stats.c_str(), stats.size() + 1);
                sleep(1);
            }
        } else if (strncmp(buff, "Set Link Up", 11) == 0) {
            if (turnIntfOn(*intf) < 0) {
                cerr << "ERROR: issue with link (set link up)" << endl;
            }
        } else if (strncmp(buff, "Shut Down", 9) == 0) {
            break;
        } 
    }

    write(intfFd, "Done", 5);
    close(intfFd);
}

int main(int argCount, char *argValues[]) 
{
    signal(SIGINT, signalHandler);
    string intf = argValues[1];
    runInterfaceMonitor(&intf);
    
    return 0;
}
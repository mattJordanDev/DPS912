#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <cstring>
#include <cstdlib>
#include <sys/select.h>

#define SOCKET_PATH "/tmp/a1.socket"
#define BUFFER_SIZE 256

using namespace std;

int monitorFd;
vector<int> intfFds;


void signalHandler(int sig) 
{
    cout << "Shutting down..." << "\n" << endl;
    
    for(size_t i = 0; i < intfFds.size(); ++i) 
    {
        write(intfFds[i], "Shut Down", 10);
    }

    close(monitorFd);
    unlink(SOCKET_PATH);

    for(size_t i = 0; i < intfFds.size(); ++i) 
    {
        close(intfFds[i]);
    }
    exit(0);
}

void runNetworkMonitor(int intfNum, const vector<string>& interfaces) 
{
    struct sockaddr_un address;
    char buff[BUFFER_SIZE];

    monitorFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(monitorFd < 0) 
    {
        cerr << "ERROR: couldnt create socket" << endl;
        exit(-1);
    }

    memset(&address, 0, sizeof(struct sockaddr_un));
    address.sun_family = AF_UNIX;
    strncpy(address.sun_path, SOCKET_PATH, sizeof(address.sun_path) - 1);

    if(bind(monitorFd, (struct sockaddr*)&address, sizeof(address)) < 0) 
    {
        cerr << "ERROR: binding didnt work" << endl;
        exit(-1);
    }

    if(listen(monitorFd, intfNum) < 0) 
    {
        cerr << "ERROR: listening not working" << endl;
        exit(-1);
    }

    cout << "Ready to connect!" << endl;

    fd_set active_fd_set;
    fd_set read_fd_set;
    FD_ZERO(&active_fd_set);
    FD_SET(monitorFd, &active_fd_set);

    int topFd = monitorFd;
    for(size_t i = 0; i < interfaces.size(); ++i) 
    {
        const string &interface = interfaces[i];
        if(fork() == 0) 
        {
            execl("./interfaceMonitor", "./interfaceMonitor", interface.c_str(), NULL);
            cerr << "ERROR: execl didnt work" << endl;
            exit(-1);
        }
    }

    while (true) 
    {
        read_fd_set = active_fd_set;
        if(select(topFd + 1, &read_fd_set, NULL, NULL, NULL) < 0) 
        {
            cerr << "ERROR: couldnt find an fd to select" << endl;
            break;
        }

        for(int i = 0; i <= topFd; ++i) 
        {
            if(FD_ISSET(i, &read_fd_set)) 
            {
                if(i == monitorFd) 
                {
                    int clientFd = accept(monitorFd, NULL, NULL);
                    if(clientFd >= 0)
                    {
                        FD_SET(clientFd, &active_fd_set);
                        if(clientFd > topFd) 
                        {
                            topFd = clientFd;
                        }
                        intfFds.push_back(clientFd);
                    }
                } else 
                {
                    int bytesRead = read(i, buff, BUFFER_SIZE);
                    if(bytesRead <= 0) 
                    {
                        if(bytesRead < 0) 
                        {
                            cerr << "ERROR: couldnt read an interfacefd (i)" << endl;
                        }
                        close(i);
                        FD_CLR(i, &active_fd_set);
                        intfFds.erase(remove(intfFds.begin(), intfFds.end(), i), intfFds.end());
                    } else 
                    {
                        buff[bytesRead] = '\0';
                        cout << buff << endl;
                        if(strncmp(buff, "Ready", 5) == 0) 
                        {
                            write(i, "Monitor", 7);
                        } else if(strncmp(buff, "Link Down", 9) == 0) 
                        {
                            write(i, "Set Link Up", 11);
                        } else if(strncmp(buff, "Done", 4) == 0) 
                        {
                            close(i);
                            FD_CLR(i, &active_fd_set);
                            intfFds.erase(remove(intfFds.begin(), intfFds.end(), i), intfFds.end());
                        }
                    }
                }
            }
        }
    }

    close(monitorFd);
    unlink(SOCKET_PATH);
    for(size_t i = 0; i < intfFds.size(); ++i) 
    {
        close(intfFds[i]);
    }
}

int main() 
{
    signal(SIGINT, signalHandler);

    int intfNum;
    cout << "Number of interfaces: ";
    cin >> intfNum;

    vector<string> interfaces(intfNum);
    for(int i = 0; i < intfNum; ++i) 
    {
        cout << "Interface name: ";
        cin >> interfaces[i];
    }

    runNetworkMonitor(intfNum, interfaces);
    return 0;
}
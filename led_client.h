#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

using namespace std;

string hostname = "192.168.1.20", port = "1222";

class LEDSys {
public:
  LEDSys(int index, string location, string msg) {
    this->msg_to_server = to_string(index) + "#" + location + "#" + msg + '\n';
    cout << "msg_to_server = " << msg_to_server << endl;
  }

  bool sendInfo() {
    int status;
    struct addrinfo host_info;
    struct addrinfo *host_info_list;

    memset(&host_info, 0, sizeof host_info);

    cout << "Setting up the structs..."  << endl;

    host_info.ai_family = AF_UNSPEC;
    host_info.ai_socktype = SOCK_STREAM;

    status = getaddrinfo(hostname.c_str(), port.c_str(), &host_info, &host_info_list);
    if (status != 0)
      {
        cout << "Getaddrinfo error" << gai_strerror(status);
        return false;}
    else {
      for (struct addrinfo *cur = host_info_list; cur; cur = cur->ai_next) {
        struct sockaddr_in* saddr = (struct sockaddr_in*)cur->ai_addr;
        printf("Hostname: %s\n", inet_ntoa(saddr->sin_addr));
      }
    }

    cout << "Creating a socket..."  << endl;
    //host_info_list = host_info_list->ai_next;
    int socketfd;
    socketfd = socket(host_info_list->ai_family, host_info_list->ai_socktype, host_info_list->ai_protocol);
    if (socketfd == -1)
    {
      cout << "Socket error ";
      return false;
    }  
      
    cout << "Connecting..."  << endl;
    status = connect(socketfd, host_info_list->ai_addr, host_info_list->ai_addrlen);
    if (status == -1)
    {
      cout << "connection error ";
      return false;
    }  
    cout << "Sending message..."  << endl;
    ssize_t bytes_sent;
    bytes_sent = send(socketfd, this->msg_to_server.c_str(), this->msg_to_server.length()+1, 0);

    freeaddrinfo(host_info_list);
    close(socketfd);
    return true;
  }

private:
  string msg_to_server = "";
};

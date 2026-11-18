#ifndef XMLSOCKET_H_INCLUDED
#define XMLSOCKET_H_INCLUDED

#include <string>
#include <unordered_set>

class XMLSocketServer
{
        constexpr static unsigned long MAX_SEND = 4096;
        int listen_socket_fd;
        unsigned listen_port;
        EventLoop looper;

        void on_listen_socket_fd_accept(int);
        void check_socket_policy_request(int fd);

    protected:
        std::unordered_set<int> remote_fds;
        void close_fd(int remote_fd);
        void send_data(int remote_fd, const std::string &buffer);

        virtual std::string get_policy_xml() = 0;
        virtual void on_new_connection(int remote_fd) = 0;
        virtual void on_socket_data(int remote_fd) = 0;

    public:
        XMLSocketServer(unsigned listen_port);
        virtual ~XMLSocketServer();

        void start_listen();
};

#endif // XMLSOCKET_H_INCLUDED

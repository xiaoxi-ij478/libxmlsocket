#ifndef XMLSOCKET_H_INCLUDED
#define XMLSOCKET_H_INCLUDED

#include <string>
#include <unordered_set>

#include <linux/sockios.h>
#include <sys/ioctl.h>

class XMLSocketServer
{
        constexpr static unsigned long MAX_SEND = 1024;
        int listen_socket_fd;
        unsigned short listen_port;
        EventLoop &looper;

        void on_listen_socket_fd_accept(int);
        void check_socket_policy_request(int fd);

    protected:
        std::unordered_set<int> remote_fds;

        static inline unsigned get_bytes_available(int remote_fd)
        {
            int val;

            if ((errno = ioctl(remote_fd, SIOCINQ, &val)))
                throw std::system_error(std::error_code(errno, std::system_category()));

            return val;
        }

        void close_fd(int remote_fd);
        void send_data(int remote_fd, const std::string &buffer);

        virtual std::string get_policy_xml();
        virtual void on_new_connection(int remote_fd) = 0;
        virtual void on_socket_data(int remote_fd) = 0;

    public:
        XMLSocketServer(EventLoop &looper, unsigned short listen_port);
        virtual ~XMLSocketServer();

        void start_listen();
};

#endif // XMLSOCKET_H_INCLUDED

#include <cstdio>
#include <cerrno>
#include <stdexcept>
#include <algorithm>
#include <functional>
#include <system_error>

#include <unistd.h>
#include <sys/socket.h>

#include "eventloop.hpp"
#include "xmlsocket.hpp"

XMLSocketServer::XMLSocketServer(unsigned listen_port) :
    listen_socket_fd(-1),
    listen_port(listen_port),
    looper(),
    remote_fds()
{
}

XMLSocketServer::~XMLSocketServer()
{
    looper.remove_fd_listener(listen_socket_fd);

    std::unordered_set<int> b_remote_fds(remote_fds);
    for (int i : b_remote_fds)
        close_fd(i);
}

void XMLSocketServer::start_listen()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd == -1)
        throw std::system_error(std::error_code(errno, std::system_category()));

    listen_socket_fd = fd;
    listen(fd, 0);
    looper.add_fd_listener(
        fd,
        std::bind(
            std::mem_fn(&XMLSocketServer::on_listen_socket_fd_accept),
            this,
            std::placeholders::_1
        )
    );
}

void XMLSocketServer::on_listen_socket_fd_accept(int)
{
    int fd = accept(listen_socket_fd, nullptr, nullptr);

    if (fd == -1)
        throw std::system_error(std::error_code(errno, std::system_category()));

    remote_fds.insert(fd);
    looper.add_fd_listener(
        fd,
        std::bind(
            std::mem_fn(&XMLSocketServer::check_socket_policy_request),
            this,
            std::placeholders::_1
        )
    );
    on_new_connection(fd);
}

void XMLSocketServer::check_socket_policy_request(int fd)
{
    /*
     * as per https://web.archive.org/web/20160308034611/http://www.adobe.com/devnet/flashplayer/articles/socket_policy_files.html#:~:text=What%20data%20is%20sent%20in%20the%20request%20and%20response
     * if we checked it's sending us <policy-file-request/>\x00 then
     * receive and close the socket
     * else
     * ignore everything and transfer the callback to subclasses
     */
    std::string buffer;
    buffer.resize(23);
    int ret = recv(fd, buffer.data(), buffer.size(), MSG_PEEK);

    if (ret == 0) {
        // shutdown? should not happen
        close_fd(fd);
        return;
    }

    if (buffer != "<policy-file-request/>\0") {
        // this is not policy file request, transfer callback
        looper.remove_fd_listener(fd);
        looper.add_fd_listener(
            fd,
            std::bind(
                std::mem_fn(&XMLSocketServer::on_socket_data),
                this,
                std::placeholders::_1
            )
        );
        return;
    }

    // is policy ... send it
    send_data(fd, get_policy_xml());
    close_fd(fd);
}

void XMLSocketServer::close_fd(int remote_fd)
{
    if (!remote_fds.count(remote_fd))
        return;

    remote_fds.erase(remote_fd);
    looper.remove_fd_listener(remote_fd);
    close(remote_fd);
}

void XMLSocketServer::send_data(int remote_fd, const std::string &buffer)
{
    for (unsigned i = 0; i < buffer.size(); i += MAX_SEND)
        send(remote_fd, buffer.c_str() + i, std::min(buffer.size() - i, MAX_SEND), 0);
}

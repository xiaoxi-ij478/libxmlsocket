#include <cstdio>
#include <cerrno>
#include <stdexcept>
#include <algorithm>
#include <functional>
#include <system_error>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <iostream>

#include "eventloop.hpp"
#include "xmlsocket.hpp"

XMLSocketServer::XMLSocketServer(
    EventLoop &looper,
    unsigned short listen_port
) :
    listen_socket_fd(-1),
    listen_port(listen_port),
    remote_fds(),
    looper(looper)
{
}

XMLSocketServer::~XMLSocketServer()
{
    if (listen_socket_fd != -1) {
        looper.remove_fd_listener(listen_socket_fd);
        close(listen_socket_fd);
    }

    for (int i : remote_fds)
        close_fd_no_delete(i);
}

void XMLSocketServer::start_listen()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd == -1)
        throw std::system_error(std::error_code(errno, std::system_category()));

    sockaddr_in sa = {
        AF_INET,
        htons(listen_port),
        inet_addr("0.0.0.0")
    };
    listen_socket_fd = fd;

    if (bind(fd, reinterpret_cast<const sockaddr *>(&sa), sizeof(sa)) == -1)
        throw std::system_error(std::error_code(errno, std::system_category()));

    if (listen(fd, 100) == -1)
        throw std::system_error(std::error_code(errno, std::system_category()));

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
    int remote_fd = accept(listen_socket_fd, nullptr, nullptr);

    if (remote_fd == -1)
        throw std::system_error(std::error_code(errno, std::system_category()));

    remote_fds.insert(remote_fd);
    looper.add_fd_listener(
        remote_fd,
        std::bind(
            std::mem_fn(&XMLSocketServer::check_socket_policy_request),
            this,
            std::placeholders::_1
        )
    );
    on_new_connection(remote_fd);
}

void XMLSocketServer::check_socket_policy_request(int remote_fd)
{
    /*
     * as per https://web.archive.org/web/20160308034611/http://www.adobe.com/devnet/flashplayer/articles/socket_policy_files.html#:~:text=What%20data%20is%20sent%20in%20the%20request%20and%20response
     * if we found it's sending us <policy-file-request/>\x00 then
     *     receive and close the socket
     * else
     *     ignore everything and transfer the callback to subclasses
     * fi
     */
    std::string buffer;
    buffer.resize(23);
    int ret = recv(remote_fd, buffer.data(), buffer.size(), MSG_PEEK);

    switch (ret) {
        case 0:
            // remote shutdown
            close_fd(remote_fd);
            return;

        case -1:
            throw std::system_error(std::error_code(errno, std::system_category()));
    }

    if (buffer == "<policy-file-request/>\0") {
        // this is policy file request, send it
        send_data(remote_fd, get_policy_xml());
        close_fd(remote_fd);
        return;
    }

    // this is not policy file request, transfer callback
    looper.remove_fd_listener(remote_fd);
    looper.add_fd_listener(
        remote_fd,
        std::bind(
            std::mem_fn(&XMLSocketServer::on_socket_data_avail),
            this,
            std::placeholders::_1
        )
    );
}

void XMLSocketServer::on_socket_data_avail(int remote_fd)
{
    on_socket_data(remote_fd);
}

void XMLSocketServer::close_fd(int remote_fd)
{
    if (!remote_fds.count(remote_fd))
        return;

    close_fd_no_delete(remote_fd);
    remote_fds.erase(remote_fd);
}

void XMLSocketServer::close_fd_no_delete(int remote_fd)
{
    if (!remote_fds.count(remote_fd))
        return;

    looper.remove_fd_listener(remote_fd);
    close(remote_fd);
    on_connection_closed(remote_fd);
}

void XMLSocketServer::send_data(int remote_fd, const std::string &buffer)
{
    for (unsigned i = 0; i < buffer.size(); i += MAX_SEND)
        if (send(remote_fd, buffer.c_str() + i, std::min(buffer.size() - i, MAX_SEND), 0) == -1)
            throw std::system_error(std::error_code(errno, std::system_category()));
}

std::string XMLSocketServer::get_policy_xml()
{
    return
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE cross-domain-policy SYSTEM \"/xml/dtds/cross-domain-policy.dtd\">\n"
        "<cross-domain-policy>\n"
        "   <site-control permitted-cross-domain-policies=\"all\"/>\n"
        "   <allow-access-from domain=\"*\"/>\n"
        "</cross-domain-policy>\n";
}

void XMLSocketServer::on_connection_closed(int)
{
}

void XMLSocketServer::on_new_connection(int)
{
}

void XMLSocketServer::on_socket_data(int)
{
}

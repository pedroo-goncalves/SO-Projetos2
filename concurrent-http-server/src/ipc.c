#include "ipc.h"
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

int send_fd(int socket_fd, int fd_to_send) {
    struct msghdr msg = {0};
    char buf[1] = {0}; // Byte dummy obrigatório
    struct iovec io = { .iov_base = buf, .iov_len = 1 };
    
    union {
        char buf[CMSG_SPACE(sizeof(int))];
        struct cmsghdr align;
    } u;

    msg.msg_iov = &io;
    msg.msg_iovlen = 1;
    msg.msg_control = u.buf;
    msg.msg_controllen = sizeof(u.buf);

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    
    memcpy(CMSG_DATA(cmsg), &fd_to_send, sizeof(int));

    if (sendmsg(socket_fd, &msg, 0) < 0) {
        perror("send_fd: sendmsg falhou");
        return -1;
    }
    return 0;
}

int recv_fd(int socket_fd) {
    struct msghdr msg = {0};
    char buf[1];
    struct iovec io = { .iov_base = buf, .iov_len = 1 };
    
    union {
        char buf[CMSG_SPACE(sizeof(int))];
        struct cmsghdr align;
    } u;

    msg.msg_iov = &io;
    msg.msg_iovlen = 1;
    msg.msg_control = u.buf;
    msg.msg_controllen = sizeof(u.buf);

    if (recvmsg(socket_fd, &msg, 0) < 0) {
        perror("recv_fd: recvmsg falhou");
        return -1;
    }

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    if (!cmsg) {
        fprintf(stderr, "recv_fd: sem dados de controlo (cmsg null)\n");
        return -1;
    }
    
    if (cmsg->cmsg_len < CMSG_LEN(sizeof(int))) {
        fprintf(stderr, "recv_fd: mensagem truncada ou incorreta\n");
        return -1;
    }

    if (cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS) {
        fprintf(stderr, "recv_fd: tipo de mensagem errado\n");
        return -1;
    }

    int fd;
    memcpy(&fd, CMSG_DATA(cmsg), sizeof(int));
    return fd;
}
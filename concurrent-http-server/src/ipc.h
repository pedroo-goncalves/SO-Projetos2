// src/ipc.h
#ifndef IPC_H
#define IPC_H
int send_fd(int socket_fd, int fd_to_send);
int recv_fd(int socket_fd);
#endif
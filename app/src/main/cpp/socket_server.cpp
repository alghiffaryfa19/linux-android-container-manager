#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <pthread.h>
#include <android/log.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

#include "common/socket_msg.h"
#include "ComposerImpl.h"

#define LOG_TAG "SocketServer"
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

extern std::shared_ptr<aidl::vendor::lindroid::composer::ComposerImpl> composer;

static int recv_fd_and_msg(int sockfd, DisplayMsg* msg) {
    struct iovec iov[1];
    struct msghdr msgh;
    union {
        struct cmsghdr cmh;
        char control[CMSG_SPACE(sizeof(int))];
    } control_un;

    iov[0].iov_base = msg;
    iov[0].iov_len = sizeof(DisplayMsg);

    msgh.msg_name = nullptr;
    msgh.msg_namelen = 0;
    msgh.msg_iov = iov;
    msgh.msg_iovlen = 1;
    msgh.msg_control = control_un.control;
    msgh.msg_controllen = sizeof(control_un.control);

    ssize_t n = recvmsg(sockfd, &msgh, 0);
    if (n <= 0) return -1;

    int fd = -1;
    struct cmsghdr *cmsg;
    for (cmsg = CMSG_FIRSTHDR(&msgh); cmsg != nullptr; cmsg = CMSG_NXTHDR(&msgh, cmsg)) {
        if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
            memcpy(&fd, CMSG_DATA(cmsg), sizeof(int));
            break;
        }
    }
    return fd;
}

static void* socket_server_thread(void* arg) {
    std::string socket_path_str = *static_cast<std::string*>(arg);
    delete static_cast<std::string*>(arg);
    const char* socket_path = socket_path_str.c_str();

    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) {
        ALOGE("Failed to create socket");
        return nullptr;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    unlink(socket_path);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(sa_family_t) + strlen(socket_path) + 1) < 0) {
        ALOGE("Failed to bind socket: %s", strerror(errno));
        close(server_fd);
        return nullptr;
    }

    if (listen(server_fd, 5) < 0) {
        ALOGE("Failed to listen: %s", strerror(errno));
        close(server_fd);
        return nullptr;
    }

    // Set permissions so the container can access it
    chmod(socket_path, 0777);

    ALOGI("Socket server listening on %s", socket_path);

    while (true) {
        int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd < 0) {
            ALOGE("Failed to accept");
            continue;
        }

        ALOGI("Client connected!");
        while (true) {
            DisplayMsg msg;
            int fd = recv_fd_and_msg(client_fd, &msg);
            if (fd == -1 && msg.cmd == 0) { // Error reading
                break;
            }

            if (msg.cmd == CMD_SET_BUFFER) {
                if (composer && fd >= 0) {
                    composer->setBufferFromFd(msg.displayId, fd, msg.width, msg.height, msg.stride, msg.format);
                } else if (fd >= 0) {
                    close(fd);
                }
            } else if (msg.cmd == CMD_CREATE_DISPLAY) {
                // Ignore for now, handled implicitly
            } else if (msg.cmd == CMD_DESTROY_DISPLAY) {
                // Handled implicitly
            }
        }
        close(client_fd);
        ALOGI("Client disconnected!");
    }

    close(server_fd);
    unlink(socket_path);
    return nullptr;
}

void start_socket_server(const char* socket_path) {
    pthread_t thread;
    std::string* path_arg = new std::string(socket_path);
    pthread_create(&thread, nullptr, socket_server_thread, path_arg);
    pthread_detach(thread);
}

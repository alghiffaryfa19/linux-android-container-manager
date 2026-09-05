#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sched.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/sysmacros.h>

#ifndef CLONE_NEWNS
#define CLONE_NEWNS 0x00020000
#endif
#ifndef CLONE_NEWPID
#define CLONE_NEWPID 0x20000000
#endif
#ifndef CLONE_NEWUTS
#define CLONE_NEWUTS 0x04000000
#endif

void log_msg(const char *msg) {
    printf("%s\n", msg);
    fflush(stdout);
}

void log_err(const char *msg) {
    printf("[ERROR] %s: %s\n", msg, strerror(errno));
    fflush(stdout);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <rootfs_path>\n", argv[0]);
        return 1;
    }

    const char *rootfs = argv[1];

    // Check if rootfs exists
    struct stat st;
    if (stat(rootfs, &st) != 0 || !S_ISDIR(st.st_mode)) {
        printf("[ERROR] RootFS directory not found at %s\n", rootfs);
        return 1;
    }

    // Unshare mount, PID, and UTS (hostname) namespaces
    if (unshare(CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWUTS) != 0) {
        perror("unshare failed");
        return 1;
    }

    // Fork to enter the new PID namespace (child will be PID 1)
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
        return 1;
    } else if (pid > 0) {
        // Parent exits immediately, child runs in the background
        return 0;
    }

    // --- We are now PID 1 in the new namespace ---

    // Redirect stdout and stderr to rootfs/container.log
    char log_path[1024];
    snprintf(log_path, sizeof(log_path), "%s/container.log", rootfs);
    int log_fd = open(log_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (log_fd >= 0) {
        dup2(log_fd, STDOUT_FILENO);
        dup2(log_fd, STDERR_FILENO);
        close(log_fd);
    }
    
    // Redirect stdin to /dev/null
    int null_fd = open("/dev/null", O_RDONLY);
    if (null_fd >= 0) {
        dup2(null_fd, STDIN_FILENO);
        close(null_fd);
    }

    log_msg("[*] Preparing Container Environment (Droidspaces style via Native C)...");
    log_msg("[*] Entering isolated mount namespace...");

    // Make all mounts private so they don't leak to host
    if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) != 0) {
        log_err("mount --make-rprivate failed");
        return 1;
    }

    // Bind mount rootfs to itself so pivot_root works
    if (mount(rootfs, rootfs, NULL, MS_BIND | MS_REC, NULL) != 0) {
        log_err("bind mount rootfs failed");
        return 1;
    }

    if (chdir(rootfs) != 0) {
        log_err("chdir rootfs failed");
        return 1;
    }

    log_msg("[*] Creating standard directories...");
    mkdir(".old_root", 0755);
    mkdir("proc", 0755);
    mkdir("sys", 0755);
    mkdir("dev", 0755);
    mkdir("tmp", 0777);
    mkdir("run", 0755);

    log_msg("[*] Mounting virtual filesystems...");
    mount("proc", "proc", "proc", 0, NULL);
    mount("sysfs", "sys", "sysfs", MS_RDONLY, NULL);
    mount("tmpfs", "tmp", "tmpfs", 0, "mode=1777");
    mount("tmpfs", "run", "tmpfs", 0, "mode=755");

    // Use an isolated tmpfs for /dev instead of host's /dev to prevent systemd-udevd from messing with Android hardware
    if (mount("tmpfs", "dev", "tmpfs", 0, "mode=755") != 0) {
        log_err("mount tmpfs on /dev failed");
    } else {
        // Create basic device nodes for systemd
        mknod("dev/null", S_IFCHR | 0666, makedev(1, 3));
        mknod("dev/zero", S_IFCHR | 0666, makedev(1, 5));
        mknod("dev/urandom", S_IFCHR | 0666, makedev(1, 9));
    }

    log_msg("[*] Pivoting root...");
    if (syscall(SYS_pivot_root, ".", ".old_root") == 0) {
        if (chdir("/") != 0) log_err("chdir / failed");
        
        // Cleanup old root
        umount2("/.old_root/sys", MNT_DETACH);
        umount2("/.old_root/proc", MNT_DETACH);
        umount2("/.old_root/dev", MNT_DETACH);
        umount2("/.old_root/tmp", MNT_DETACH);
        umount2("/.old_root/run", MNT_DETACH);
        umount2("/.old_root", MNT_DETACH);
        rmdir("/.old_root");
    } else {
        log_msg("[*] pivot_root failed, falling back to chroot");
        if (chroot(".") != 0) {
            log_err("chroot failed");
            return 1;
        }
        if (chdir("/") != 0) log_err("chdir / failed");
    }

    log_msg("[*] Container Started Successfully!");

    // Execute the init system
    // Tell systemd that it's running in a container, otherwise it might try to reboot the host
    char *init_args[] = {"/sbin/init", NULL};
    char *init_env[] = {
        "container=lxc",
        "PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin",
        "TERM=xterm",
        "HOME=/root",
        "USER=root",
        NULL
    };
    execve("/sbin/init", init_args, init_env);
    
    // If execve fails
    log_err("execve /sbin/init failed");
    return 1;
}

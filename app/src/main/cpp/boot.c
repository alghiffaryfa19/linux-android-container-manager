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

    if (argc >= 3 && strcmp(argv[2], "enter") == 0) {
        char pid_path[1024];
        snprintf(pid_path, sizeof(pid_path), "%s/container.pid", rootfs);
        FILE *f = fopen(pid_path, "r");
        if (!f) {
            printf("[ERROR] Container is not running (no container.pid)\n");
            return 1;
        }
        int init_pid;
        if (fscanf(f, "%d", &init_pid) != 1) {
            printf("[ERROR] Invalid container.pid\n");
            fclose(f);
            return 1;
        }
        fclose(f);
        
        printf("[*] Entering container namespaces (PID %d)...\n", init_pid);
        
        char ns_path[256];
        const char *namespaces[] = {"user", "ipc", "uts", "net", "pid", "mnt", "cgroup"};
        for (int i = 0; i < 7; i++) {
            snprintf(ns_path, sizeof(ns_path), "/proc/%d/ns/%s", init_pid, namespaces[i]);
            int fd = open(ns_path, O_RDONLY);
            if (fd >= 0) {
                setns(fd, 0);
                close(fd);
            }
        }
        
        pid_t child = fork();
        if (child == 0) {
            chdir("/");
            char *sh_env[] = {
                "container=lxc",
                "PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin",
                "TERM=xterm",
                "HOME=/root",
                "USER=root",
                NULL
            };
            char *bash_args[] = {"/bin/bash", NULL};
            execve("/bin/bash", bash_args, sh_env);
            char *sh_args[] = {"/bin/sh", NULL};
            execve("/bin/sh", sh_args, sh_env);
            return 1;
        } else if (child > 0) {
            int status;
            waitpid(child, &status, 0);
            return 0;
        } else {
            perror("fork");
            return 1;
        }
    }

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
        // Parent
        char pid_path[1024];
        snprintf(pid_path, sizeof(pid_path), "%s/container.pid", rootfs);
        FILE *f = fopen(pid_path, "w");
        if (f) {
            fprintf(f, "%d\n", pid);
            fclose(f);
        }

        if (argc > 2) {
            // CLI mode: Wait for the child so the terminal session stays active
            int status;
            waitpid(pid, &status, 0);
        }
        return 0;
    }

    // --- We are now PID 1 in the new namespace ---

    // Redirect stdout and stderr to rootfs/container.log ONLY if in background mode
    if (argc <= 2) {
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
    
    // Allow create-disp to write to LINDROID EVDI virtual display interface safely
    struct stat evdi_st;
    if (stat("/sys/devices/evdi-lindroid", &evdi_st) == 0) {
        mount("/sys/devices/evdi-lindroid", "sys/devices/evdi-lindroid", NULL, MS_BIND, NULL);
    }
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
        
        // Setup devpts for terminal emulators and apt
        mkdir("dev/pts", 0755);
        if (mount("devpts", "dev/pts", "devpts", 0, "newinstance,ptmxmode=0666") != 0) {
            log_err("mount devpts failed");
        }
        symlink("pts/ptmx", "dev/ptmx");
        
        // Setup GPU, Binder, and LINDROID Hardware nodes from host safely
        const char *gpu_nodes[] = {
            "/dev/dri", "/dev/kgsl-3d0", "/dev/mali0", "/dev/pvr_sync", "/dev/ion", "/dev/dma_heap",
            "/dev/socket", "/dev/__properties__", "/dev/binder", "/dev/hwbinder", "/dev/vndbinder",
            "/dev/pmsg0", "/dev/ashmem", "/dev/input", "/dev/binderfs"
        };
        for (int i = 0; i < 15; i++) {
            struct stat s;
            if (stat(gpu_nodes[i], &s) == 0) {
                char dest[256];
                snprintf(dest, sizeof(dest), "dev/%s", gpu_nodes[i] + 5);
                if (S_ISDIR(s.st_mode)) {
                    mkdir(dest, 0755);
                } else {
                    int fd = open(dest, O_CREAT | O_WRONLY, 0666);
                    if (fd >= 0) close(fd);
                }
                mount(gpu_nodes[i], dest, NULL, MS_BIND | MS_REC, NULL);
            }
        }
    }

    log_msg("[*] Mounting host Android partitions for LINDROID libhybris...");
    const char *host_parts[] = {
        "/system", "/vendor", "/vendor_dlkm", "/system_dlkm", 
        "/system_ext", "/product", "/odm", "/odm_dlkm", "/apex"
    };
    for (int i = 0; i < 9; i++) {
        struct stat s;
        if (stat(host_parts[i], &s) == 0) {
            char dest[256];
            snprintf(dest, sizeof(dest), "%s", host_parts[i] + 1); // skip leading '/'
            mkdir(dest, 0755);
            // Just bind mount, avoid MS_REMOUNT to prevent kernel panic
            mount(host_parts[i], dest, NULL, MS_BIND | MS_REC, NULL);
        }
    }

    // LINDROID Quirk: Bind mount patched libc inside container to avoid errors
    const char *patched_libc = NULL;
    struct stat libc_st;
    if (stat("/system_ext/usr/share/lindroid/libc.so", &libc_st) == 0) {
        patched_libc = "/system_ext/usr/share/lindroid/libc.so";
    } else if (stat("/data/user/0/com.fauzan.containermanager/files/libc.so", &libc_st) == 0) { // Built-in from App
        patched_libc = "/data/user/0/com.fauzan.containermanager/files/libc.so";
    } else if (stat("usr/share/lindroid/libc.so", &libc_st) == 0) { // inside rootfs
        patched_libc = "usr/share/lindroid/libc.so";
    } else if (stat("opt/lindroid/libc.so", &libc_st) == 0) { // inside rootfs
        patched_libc = "opt/lindroid/libc.so";
    }

    if (patched_libc) {
        mount(patched_libc, "apex/com.android.runtime/lib64/bionic/libc.so", NULL, MS_BIND, NULL);
        log_msg("[*] Applied LINDROID libc quirk for libhybris/vulkan-bridge");
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
    char **init_args;
    char *init_path;
    if (argc > 2) {
        init_args = &argv[2];
        init_path = argv[2];
    } else {
        static char *default_init[] = {"/sbin/init", NULL};
        init_args = default_init;
        init_path = "/sbin/init";
    }

    // Tell systemd that it's running in a container, otherwise it might try to reboot the host
    char *init_env[] = {
        "container=lxc",
        "PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin",
        "TERM=xterm",
        "HOME=/root",
        "USER=root",
        NULL
    };
    execve(init_path, init_args, init_env);
    
    // If execve fails
    log_err("execve failed");
    return 1;
}

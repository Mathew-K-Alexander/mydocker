#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <stdbool.h>

#define STACK_SIZE (1024 * 1024)
#define CGROUPS "/sys/fs/cgroup/"
#define PID_LIMIT "20"
#define CGROUP_NAME "mathew"

void setup_cgroup(void);
void write_to_file(const char *path, const char *value);

int child(void *arg) {
	printf("Inside new UTS namespace\n");

	umount("/home/alexa/container-rootfs/proc");
	setup_cgroup();
	if (sethostname("container", 10) == -1) {
			perror("sethostname failed");
			exit(1);
	}

	if (mount("proc", "/home/alexa/container-rootfs/proc", "proc", 0, NULL) == -1) {
		perror("mount proc failed");
	}

	if (chroot("/home/alexa/container-rootfs") == -1) {
			perror("chroot failed");
			exit(1);
	}

	if (chdir("/") == -1) {
			perror("chdir failed");
			exit(1);
	}


	char **cmd = (char **)arg;
	execvp(cmd[2], &cmd[2]);

	printf("Failed to execute %s: %s\n", cmd[2], strerror(errno));

	printf("Trying fallback to bash...\n");
	execlp("/bin/bash", "bash", NULL);

	perror("execlp failed");
	exit(1);
}

int run(int argc, char *argv[]){
	printf("Running ");
	for (int i=2; i< argc; i++){
		printf(" %s",argv[i]);
	}
	printf("\n");

	char *stack = malloc(STACK_SIZE);
    if (!stack) {
        perror("malloc failed");
        exit(1);
    }

	int flags = CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWNS | SIGCHLD;
	pid_t pid = clone(child, stack+STACK_SIZE-1, flags, argv);

	if (pid==-1){
		perror("clone failed\n");
		free(stack);
		exit(1);
	}

	waitpid(pid,NULL,0);

	free(stack);
	return 0;
}

void write_to_file(const char *path, const char *value) {
    int fd = open(path, O_WRONLY);
    if (fd == -1) {
        perror(path);
        exit(EXIT_FAILURE);
    }
    if (write(fd, value, strlen(value)) == -1) {
        perror("Failed to write to file");
        close(fd);
        exit(EXIT_FAILURE);
    }
    close(fd);
}


void setup_cgroup() {
    char path[256];
    struct stat st;

    bool is_cgv2 = (stat("/sys/fs/cgroup/cgroup.controllers", &st) == 0);
    const char *base_path = is_cgv2 ?
        "/sys/fs/cgroup/mathew" :
        "/sys/fs/cgroup/pids/mathew";

    if (mkdir(base_path, 0755) == -1 && errno != EEXIST) {
        perror("Failed to create cgroup directory");
        exit(EXIT_FAILURE);
    }

    // Step 1: Set pids.max
    snprintf(path, sizeof(path), "%s/pids.max", base_path);
    write_to_file(path, PID_LIMIT);

    // Step 2: v1 only: notify_on_release
    if (!is_cgv2) {
        snprintf(path, sizeof(path), "%s/notify_on_release", base_path);
        write_to_file(path, "1");
    }

    // Step 3: Move current (child/container) process into cgroup
    snprintf(path, sizeof(path), "%s/cgroup.procs", base_path);
    char pid_str[32];
    snprintf(pid_str, sizeof(pid_str), "%d", getpid());
    write_to_file(path, pid_str);
}


int main(int argc, char *argv[]){
	if (argc > 2 && strcmp(argv[1],"run")==0){
		run(argc,argv);
	}
	else{
		printf("Bad command\n");
}
	return 0;
}


#include "application.h"
#include "ui/window.h"

#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#define RV_ELEVATION_FD_ENV "RVKERNEL_ELEVATION_FD"

static void
add_env_args(GPtrArray *args)
{
        const gchar *keys[] = { "DISPLAY", "WAYLAND_DISPLAY", "XDG_RUNTIME_DIR",
                                "XAUTHORITY", "GDK_BACKEND", NULL };
        gsize i;

        for (i = 0; keys[i] != NULL; i++) {
                const gchar *val = g_getenv(keys[i]);

                if (val != NULL)
                        g_ptr_array_add(args,
                                        g_strdup_printf("%s=%s", keys[i], val));
        }
}

static void
report_elevation_ready(void)
{
        const gchar *fd_str;
        gchar b = 'R';
        gssize nwritten;
        gint fd;

        if (geteuid() != 0)
                return;

        fd_str = g_getenv(RV_ELEVATION_FD_ENV);
        if (fd_str == NULL)
                return;

        g_unsetenv(RV_ELEVATION_FD_ENV);

        fd = atoi(fd_str);
        if (fd <= 0 || fcntl(fd, F_GETFD) == -1) {
                if (fd > 0)
                        close(fd);
                return;
        }

        nwritten = write(fd, &b, 1);
        (void)nwritten;
        close(fd);
}

static gboolean
try_elevate(const gchar *pkexec, int *exit_status)
{
        GPtrArray *args;
        gchar *exe, *fd_arg;
        gint fds[2];
        pid_t pid;
        gint status = -1;
        gchar ready = 0;
        gssize nread = 0;

        *exit_status = 0;

        if (pipe(fds) != 0)
                return FALSE;

        exe = g_file_read_link("/proc/self/exe", NULL);
        if (exe == NULL)
                exe = g_strdup(program_invocation_name);

        fd_arg = g_strdup_printf("%s=%d", RV_ELEVATION_FD_ENV, fds[1]);

        args = g_ptr_array_new_with_free_func(g_free);
        g_ptr_array_add(args, g_strdup(pkexec));
        g_ptr_array_add(args, g_strdup("env"));
        add_env_args(args);
        g_ptr_array_add(args, fd_arg);
        g_ptr_array_add(args, exe);
        g_ptr_array_add(args, NULL);

        pid = fork();
        if (pid == 0) {
                close(fds[0]);
                execv(pkexec, (char * const *)args->pdata);
                _exit(127);
        }
        close(fds[1]);
        g_ptr_array_unref(args);

        if (pid < 0) {
                close(fds[0]);
                return FALSE;
        }

        nread = read(fds[0], &ready, 1);
        close(fds[0]);
        waitpid(pid, &status, 0);

        if (nread != 1) {
                if (WIFEXITED(status))
                        *exit_status = WEXITSTATUS(status);
                else if (WIFSIGNALED(status))
                        *exit_status = 128 + WTERMSIG(status);
                else
                        *exit_status = 1;
                return FALSE;
        }

        if (WIFEXITED(status))
                *exit_status = WEXITSTATUS(status);
        else if (WIFSIGNALED(status))
                *exit_status = 128 + WTERMSIG(status);
        else
                *exit_status = 1;

        return TRUE;
}

static void
elevate_if_needed(void)
{
        gchar *pkexec;
        int exit_status = 0;

        if (geteuid() == 0)
                return;

        pkexec = g_find_program_in_path("pkexec");
        if (pkexec == NULL) {
                g_printerr("%s: root access required "
                           "(polkit's pkexec not found)\n",
                           RV_APP_NAME);
                _exit(1);
        }

        if (try_elevate(pkexec, &exit_status))
                _exit(exit_status);

        if (exit_status == 0)
                exit_status = 1;
        _exit(exit_status);
}

int
main(int argc, char **argv)
{
        GtkApplication *app;
        int status;

        elevate_if_needed();
        report_elevation_ready();

        app = rv_application_new();
        status = g_application_run(G_APPLICATION(app), argc, argv);
        g_object_unref(app);

        return status;
}

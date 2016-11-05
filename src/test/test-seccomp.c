/***
  This file is part of systemd.

  Copyright 2016 Lennart Poettering

  systemd is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation; either version 2.1 of the License, or
  (at your option) any later version.

  systemd is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with systemd; If not, see <http://www.gnu.org/licenses/>.
***/

#include <stdlib.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include "fd-util.h"
#include "macro.h"
#include "process-util.h"
#include "seccomp-util.h"
#include "string-util.h"
#include "util.h"

static void test_seccomp_arch_to_string(void) {
        uint32_t a, b;
        const char *name;

        a = seccomp_arch_native();
        assert_se(a > 0);
        name = seccomp_arch_to_string(a);
        assert_se(name);
        assert_se(seccomp_arch_from_string(name, &b) >= 0);
        assert_se(a == b);
}

static void test_architecture_table(void) {
        const char *n, *n2;

        NULSTR_FOREACH(n,
                       "native\0"
                       "x86\0"
                       "x86-64\0"
                       "x32\0"
                       "arm\0"
                       "arm64\0"
                       "mips\0"
                       "mips64\0"
                       "mips64-n32\0"
                       "mips-le\0"
                       "mips64-le\0"
                       "mips64-le-n32\0"
                       "ppc\0"
                       "ppc64\0"
                       "ppc64-le\0"
                       "s390\0"
                       "s390x\0") {
                uint32_t c;

                assert_se(seccomp_arch_from_string(n, &c) >= 0);
                n2 = seccomp_arch_to_string(c);
                log_info("seccomp-arch: %s → 0x%"PRIx32" → %s", n, c, n2);
                assert_se(streq_ptr(n, n2));
        }
}

static void test_syscall_filter_set_find(void) {
        assert_se(!syscall_filter_set_find(NULL));
        assert_se(!syscall_filter_set_find(""));
        assert_se(!syscall_filter_set_find("quux"));
        assert_se(!syscall_filter_set_find("@quux"));

        assert_se(syscall_filter_set_find("@clock") == syscall_filter_sets + SYSCALL_FILTER_SET_CLOCK);
        assert_se(syscall_filter_set_find("@default") == syscall_filter_sets + SYSCALL_FILTER_SET_DEFAULT);
        assert_se(syscall_filter_set_find("@raw-io") == syscall_filter_sets + SYSCALL_FILTER_SET_RAW_IO);
}

static void test_filter_sets(void) {
        unsigned i;
        int r;

        if (!is_seccomp_available())
                return;

        if (geteuid() != 0)
                return;

        for (i = 0; i < _SYSCALL_FILTER_SET_MAX; i++) {
                pid_t pid;

                log_info("Testing %s", syscall_filter_sets[i].name);

                pid = fork();
                assert_se(pid >= 0);

                if (pid == 0) { /* Child? */
                        int fd;

                        if (i == SYSCALL_FILTER_SET_DEFAULT) /* if we look at the default set, whitelist instead of blacklist */
                                r = seccomp_load_filter_set(SCMP_ACT_ERRNO(EPERM), syscall_filter_sets + i, SCMP_ACT_ALLOW);
                        else
                                r = seccomp_load_filter_set(SCMP_ACT_ALLOW, syscall_filter_sets + i, SCMP_ACT_ERRNO(EPERM));
                        if (r < 0)
                                _exit(EXIT_FAILURE);

                        /* Test the sycall filter with one random system call */
                        fd = eventfd(0, EFD_NONBLOCK|EFD_CLOEXEC);
                        if (IN_SET(i, SYSCALL_FILTER_SET_IO_EVENT, SYSCALL_FILTER_SET_DEFAULT))
                                assert_se(fd < 0 && errno == EPERM);
                        else {
                                assert_se(fd >= 0);
                                safe_close(fd);
                        }

                        _exit(EXIT_SUCCESS);
                }

                assert_se(wait_for_terminate_and_warn(syscall_filter_sets[i].name, pid, true) == EXIT_SUCCESS);
        }
}

int main(int argc, char *argv[]) {

        test_seccomp_arch_to_string();
        test_architecture_table();
        test_syscall_filter_set_find();
        test_filter_sets();

        return 0;
}

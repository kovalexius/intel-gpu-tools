/*
 * Copyright © 2007 Intel Corporation
 * Copyright © 2011 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Eric Anholt <eric@anholt.net>
 *    Eugeni Dodonov <eugeni.dodonov@intel.com>
 *    Alexandr Kovalev <saneck2ru@netup.ru>
 *
 */

#include <iostream>
#include <thread>
#include <atomic>
#include <mutex>
#include <cstdlib>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <poll.h>

#include <sys/socket.h>
#include <fcgi_stdio.h>

extern "C"
{
#include "netup_get_statistics.h"
}

static void
usage(const char *appname)
{
    std::cout << "intel_gpu_top - Display a top-like summary of Intel GPU usage" << std::endl <<
            std::endl <<
            "usage: " << appname << " [parameters]" << std::endl <<
            std::endl <<
            "The following parameters apply:" << std::endl <<
            "[-s <samples>]       samples per seconds (default " << samples_per_sec << ")" << std::endl << 
            "[-h]                 show this help screen" << std::endl <<
            std::endl;

    return;
}

std::mutex params_mutex;
std::atomic<bool> app_run(true);
void do_stop(int signl)
{
    std::cout << std::endl << "Stop signal code: " << signl << std::endl;
    app_run.store(false);
}

void get_device_statistics()
{
    init_device();
    while(app_run.load())
    {
        get_device_params();
        {
            std::lock_guard<std::mutex> lock(params_mutex);
            print_device_params();
        }
        reset_params_values();
    }
    deinit_device();
}

int main(int argc, char **argv)
{   
    int ch;
    while ((ch = getopt(argc, argv, "s:h")) != -1) 
    {
        switch (ch) 
        {
        case 's': samples_per_sec = atoi(optarg);
            if (samples_per_sec < 100) 
            {
                fprintf(stderr, "Error: samples per second must be >= 100\n");
                exit(1);
            }
            break;
        case 'h':
            usage(argv[0]);
            exit(0);
            break;
        default:
            fprintf(stderr, "Invalid flag %c!\n", (char)optopt);
            usage(argv[0]);
            exit(1);
            break;
        }
    }

    auto previous_handler = signal(SIGABRT, do_stop);
    if(previous_handler == SIG_ERR)
    {
        std::cerr << "Setup SIGABRT failed\n";
        return EXIT_FAILURE;
    }
    signal(SIGTERM, do_stop);
    if(previous_handler == SIG_ERR)
    {
        std::cerr << "Setup SIGTERM failed\n";
        return EXIT_FAILURE;
    }
    signal(SIGINT , do_stop);
    if(previous_handler == SIG_ERR)
    {
        std::cerr << "Setup SIGINT failed\n";
        return EXIT_FAILURE;
    }
    
    std::thread device_thread(get_device_statistics);
    
    FCGX_Init();
    FCGX_Request request;
    struct pollfd pfd;
    pfd.events = POLL_IN;
    pfd.fd = 0;                          // The listen socket's file descriptor in a process spawned by the mod_fastcgi
                                         // process manager is always 0 (zero)
    FCGX_InitRequest(&request, 0, 0);
    while(app_run)
    {
        int res = ::poll(&pfd, 1, 50);
        if(res < 0)
            break;
        if(res == 0 || FCGX_Accept_r(&request) != 0)
            continue;
        {
            std::lock_guard<std::mutex> lock(params_mutex);
            FCGX_PutStr(out_buffer, strlen(out_buffer), request.out);
        }
        FCGX_Finish_r(&request);
    }
    
    device_thread.join();

    return 0;
}

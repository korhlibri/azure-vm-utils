/**
 * Copyright (c) Microsoft Corporation. All rights reserved.
 * Licensed under the MIT License. See LICENSE in the project root for license
 * information.
 */

#include <getopt.h>
#include <string.h>

#include "debug.h"
#include "identify_disks.h"
#include "identify_udev.h"
#include "version.h"

void print_help(const char *program)
{
    printf("Usage: %s [-d|--debug] [-u|--udev|-h|--help|-v|--version]\n", program);
    printf("  -d, --debug    Enable debug mode\n");
    printf("  -u, --udev     Enable udev mode\n");
    printf("  -h, --help     Display this help message\n");
    printf("  -v, --version  Display the version\n");
}

void print_version(const char *program)
{
    printf("%s %s\n", program, VERSION);
}

void print_invalid_argument(const char *program, const char *argument)
{
    fprintf(stderr, "invalid argument: %s\n", argument);
    print_help(program);
}

int main(int argc, char **argv)
{
    bool udev_mode = false;

    int opt;
    int option_index = 0;

    static struct option long_options[] = {{"debug", no_argument, 0, 'd'},
                                           {"udev", no_argument, 0, 'u'},
                                           {"version", no_argument, 0, 'v'},
                                           {"help", no_argument, 0, 'h'},
                                           {0, 0, 0, 0}};

    while ((opt = getopt_long(argc, argv, "duvh", long_options, &option_index)) != -1)
    {
        switch (opt)
        {
        case 'd':
            debug = true;
            break;
        case 'u':
            udev_mode = true;
            break;
        case 'v':
            print_version(argv[0]);
            return 0;
        case 'h':
            print_help(argv[0]);
            return 0;
        default:
            // Error for invalid --args
            print_invalid_argument(argv[0], argv[optind - 1]);
            return 1;
        }
    }

    // Error for unparsed args
    if (optind < argc)
    {
        print_invalid_argument(argv[0], argv[optind]);
        return 1;
    }

    if (debug)
    {
        debug_environment_variables();
    }

    if (udev_mode)
    {
        return identify_udev_device();
    }

    return identify_disks();
}

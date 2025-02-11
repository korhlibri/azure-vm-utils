#include <dirent.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// clang-format off
#include <cmocka.h>
// clang-format on

#include "capture.h"
#include "identify_disks.h"

#define MICROSOFT_NVME_DIRECT_DISK_V1 "Microsoft NVMe Direct Disk              \n"
#define MICROSOFT_NVME_DIRECT_DISK_V2 "Microsoft NVMe Direct Disk v2           \n"
#define MSFT_NVME_ACCELERATOR_MODEL_V1 "MSFT NVMe Accelerator v1.0              \n"

char *fake_sys_class_nvme_path = "/sys/class/nvme";

char *__wrap_nvme_identify_namespace_vs_for_namespace_device(const char *namespace_path)
{
    check_expected_ptr(namespace_path);
    return mock_ptr_type(char *);
}

static void remove_temp_dir(const char *path)
{
    struct dirent *entry;
    DIR *dir = opendir(path);

    if (dir == NULL)
    {
        return;
    }

    while ((entry = readdir(dir)) != NULL)
    {
        char full_path[MAX_PATH];
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        if (entry->d_type == DT_DIR)
        {
            remove_temp_dir(full_path);
        }
        else
        {
            remove(full_path);
        }
    }

    closedir(dir);
    rmdir(path);
}

static void create_intermediate_dirs(const char *path)
{
    char temp_path[MAX_PATH];
    snprintf(temp_path, sizeof(temp_path), "%s", path);

    char *p = temp_path;
    while (*p)
    {
        if (*p == '/')
        {
            *p = '\0';
            mkdir(temp_path, 0755);
            *p = '/';
        }
        p++;
    }
}

static void create_dir(const char *base_path, const char *sub_path)
{
    char full_path[MAX_PATH];
    snprintf(full_path, sizeof(full_path), "%s/%s", base_path, sub_path);
    create_intermediate_dirs(full_path);
    mkdir(full_path, 0755);
}

static void create_file(const char *base_path, const char *sub_path, const char *content)
{
    char full_path[MAX_PATH];
    snprintf(full_path, sizeof(full_path), "%s/%s", base_path, sub_path);
    create_intermediate_dirs(full_path);

    FILE *file = fopen(full_path, "w");
    if (file == NULL)
    {
        perror("fopen");
        return;
    }
    fputs(content, file);
    fclose(file);
}

static int setup(void **state)
{
    capture_setup(state);

    char template[] = "/tmp/nvme_test_XXXXXX";
    char *temp_path = mkdtemp(template);
    if (temp_path == NULL)
    {
        perror("mkdtemp");
        return -1;
    }

    *state = strdup(temp_path);
    fake_sys_class_nvme_path = *state;

    return 0;
}

static int teardown(void **state)
{
    capture_teardown(state);

    if (*state != NULL)
    {
        remove_temp_dir(*state);
        free(*state);
        *state = NULL;
    }

    fake_sys_class_nvme_path = SYS_CLASS_NVME_PATH;
    return 0;
}

/**
 * Setup nvme0: microsoft disk controler, no namespaces.
 */
static void _setup_nvme0_microsoft_no_name_namespaces(void)
{
    create_file(fake_sys_class_nvme_path, "nvme0/device/vendor", "0x1414");

    // nothing to expect as no namespaces are iterated through.
}

/**
 * Setup nvme1: microsoft disk, one namespace.
 */
static void _setup_nvme1_microsoft_one_namespace(void)
{
    create_file(fake_sys_class_nvme_path, "nvme1/device/vendor", "0x1414");
    create_dir(fake_sys_class_nvme_path, "nvme1/nvme1n1");

    expect_string(__wrap_nvme_identify_namespace_vs_for_namespace_device, namespace_path, "/dev/nvme1n1");
    will_return(__wrap_nvme_identify_namespace_vs_for_namespace_device,
                strdup("key1=nvme1n1value1,key2=nvme1n1value2"));
}

/**
 * Setup nvme2: microsoft disk, two namespaces.
 */
static void _setup_nvme2_microsoft_two_namespaces(void)
{
    create_file(fake_sys_class_nvme_path, "nvme2/device/vendor", "0x1414");
    create_dir(fake_sys_class_nvme_path, "nvme2/nvme2n1");
    create_dir(fake_sys_class_nvme_path, "nvme2/nvme2n2");

    expect_string(__wrap_nvme_identify_namespace_vs_for_namespace_device, namespace_path, "/dev/nvme2n1");
    will_return(__wrap_nvme_identify_namespace_vs_for_namespace_device,
                strdup("key1=nvme2n1value1,key2=nvme2n1value2"));
    expect_string(__wrap_nvme_identify_namespace_vs_for_namespace_device, namespace_path, "/dev/nvme2n2");
    will_return(__wrap_nvme_identify_namespace_vs_for_namespace_device,
                strdup("key1=nvme2n2value1,key2=nvme2n2value2"));
}

// No nvme3.

/**
 * Setup nvme4: non-microsoft disk.
 */
static void _setup_nvme4_non_microsoft(void)
{
    create_file(fake_sys_class_nvme_path, "nvme4/device/vendor", "0x0000");
    create_dir(fake_sys_class_nvme_path, "nvme4/nvme4n1");
    create_dir(fake_sys_class_nvme_path, "nvme4/nvme4n2");
}

/**
 * Setup nvme5: microsoft disk, empty vs for nsid=2, error on nsid=3.
 */
static void _setup_nvme5_microsoft_mixed_namespaces(void)
{
    create_file(fake_sys_class_nvme_path, "nvme5/device/vendor", "0x1414");
    create_file(fake_sys_class_nvme_path, "nvme5/model", "Unknown model");
    create_dir(fake_sys_class_nvme_path, "nvme5/nvme5n1");
    create_dir(fake_sys_class_nvme_path, "nvme5/nvme5n2");
    create_dir(fake_sys_class_nvme_path, "nvme5/nvme5n3");
    create_dir(fake_sys_class_nvme_path, "nvme5/nvme5n4");

    expect_string(__wrap_nvme_identify_namespace_vs_for_namespace_device, namespace_path, "/dev/nvme5n1");
    will_return(__wrap_nvme_identify_namespace_vs_for_namespace_device,
                strdup("key1=nvme5n1value1,key2=nvme5n1value2"));
    expect_string(__wrap_nvme_identify_namespace_vs_for_namespace_device, namespace_path, "/dev/nvme5n2");
    will_return(__wrap_nvme_identify_namespace_vs_for_namespace_device, strdup(""));
    expect_string(__wrap_nvme_identify_namespace_vs_for_namespace_device, namespace_path, "/dev/nvme5n3");
    will_return(__wrap_nvme_identify_namespace_vs_for_namespace_device, NULL);
    expect_string(__wrap_nvme_identify_namespace_vs_for_namespace_device, namespace_path, "/dev/nvme5n4");
    will_return(__wrap_nvme_identify_namespace_vs_for_namespace_device,
                strdup("key1=nvme5n4value1,key2=nvme5n4value2"));
}

/**
 * Setup nvme6: remote accelerator v1 with vs.
 */
static void _setup_nvme6_remote_accelerator_v1_with_vs(void)
{
    create_file(fake_sys_class_nvme_path, "nvme6/device/vendor", "0x1414");
    create_dir(fake_sys_class_nvme_path, "nvme6/nvme6n1");
    create_file(fake_sys_class_nvme_path, "nvme6/model", MSFT_NVME_ACCELERATOR_MODEL_V1);

    expect_string(__wrap_nvme_identify_namespace_vs_for_namespace_device, namespace_path, "/dev/nvme6n1");
    will_return(__wrap_nvme_identify_namespace_vs_for_namespace_device,
                strdup("key1=nvme6n1value1,key2=nvme6n1value2"));
}

/**
 * Setup nvme7: remote accelerator v1 without vs.
 */
static void _setup_nvme7_remote_accelerator_v1_without_vs(void)
{
    create_file(fake_sys_class_nvme_path, "nvme7/device/vendor", "0x1414");
    create_file(fake_sys_class_nvme_path, "nvme7/model", MSFT_NVME_ACCELERATOR_MODEL_V1);
    create_dir(fake_sys_class_nvme_path, "nvme7/nvme7n1");
    create_dir(fake_sys_class_nvme_path, "nvme7/nvme7n2");
    create_dir(fake_sys_class_nvme_path, "nvme7/nvme7n3");
    create_dir(fake_sys_class_nvme_path, "nvme7/nvme7n4");
    create_dir(fake_sys_class_nvme_path, "nvme7/nvme7n9");

    expect_string(__wrap_nvme_identify_namespace_vs_for_namespace_device, namespace_path, "/dev/nvme7n1");
    will_return(__wrap_nvme_identify_namespace_vs_for_namespace_device, strdup(""));
    expect_string(__wrap_nvme_identify_namespace_vs_for_namespace_device, namespace_path, "/dev/nvme7n2");
    will_return(__wrap_nvme_identify_namespace_vs_for_namespace_device, strdup(""));
    expect_string(__wrap_nvme_identify_namespace_vs_for_namespace_device, namespace_path, "/dev/nvme7n3");
    will_return(__wrap_nvme_identify_namespace_vs_for_namespace_device, strdup(""));
    expect_string(__wrap_nvme_identify_namespace_vs_for_namespace_device, namespace_path, "/dev/nvme7n4");
    will_return(__wrap_nvme_identify_namespace_vs_for_namespace_device, strdup(""));
    expect_string(__wrap_nvme_identify_namespace_vs_for_namespace_device, namespace_path, "/dev/nvme7n9");
    will_return(__wrap_nvme_identify_namespace_vs_for_namespace_device, strdup(""));
}

/**
 * Setup nvme8: direct disk v1 which doesn't have vs support.
 */
static void _setup_nvme8_direct_disk_v1_without_vs(void)
{
    create_file(fake_sys_class_nvme_path, "nvme8/device/vendor", "0x1414");
    create_file(fake_sys_class_nvme_path, "nvme8/model", MICROSOFT_NVME_DIRECT_DISK_V1);
    create_dir(fake_sys_class_nvme_path, "nvme8/nvme8n1");

    expect_string(__wrap_nvme_identify_namespace_vs_for_namespace_device, namespace_path, "/dev/nvme8n1");
    will_return(__wrap_nvme_identify_namespace_vs_for_namespace_device, strdup(""));
}

/**
 * Setup nvme9: direct disk v2 which has vs support.
 */
static void _setup_nvme9_direct_disk_v2(void)
{
    create_file(fake_sys_class_nvme_path, "nvme9/device/vendor", "0x1414");
    create_file(fake_sys_class_nvme_path, "nvme9/model", MICROSOFT_NVME_DIRECT_DISK_V2);
    create_dir(fake_sys_class_nvme_path, "nvme9/nvme9n1");

    expect_string(__wrap_nvme_identify_namespace_vs_for_namespace_device, namespace_path, "/dev/nvme9n1");
    will_return(__wrap_nvme_identify_namespace_vs_for_namespace_device,
                strdup("key1=nvme9n1value1,key2=nvme9n1value2"));
}

/**
 * Setup nvme10: direct disk v2 which is missing vs support (unexpected case).
 */
static void _setup_nvme10_direct_disk_v2_missing_vs(void)
{
    create_file(fake_sys_class_nvme_path, "nvme10/device/vendor", "0x1414");
    create_file(fake_sys_class_nvme_path, "nvme10/model", MICROSOFT_NVME_DIRECT_DISK_V2);
    create_dir(fake_sys_class_nvme_path, "nvme10/nvme10n1");

    expect_string(__wrap_nvme_identify_namespace_vs_for_namespace_device, namespace_path, "/dev/nvme10n1");
    will_return(__wrap_nvme_identify_namespace_vs_for_namespace_device, strdup(""));
}

static void test_trim_trailing_whitespace(void **state)
{
    (void)state; // Unused parameter

    struct
    {
        char input[MAX_PATH];
        char expected[MAX_PATH];
    } test_cases[] = {{"NoTrailingWhitespace", "NoTrailingWhitespace"},
                      {"TrailingSpaces   ", "TrailingSpaces"},
                      {"TrailingTabs\t\t\t", "TrailingTabs"},
                      {"TrailingNewline\n", "TrailingNewline"},
                      {"TrailingMixed   \t\n", "TrailingMixed"},
                      {"", ""},
                      {"\0", "\0"}};

    for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++)
    {
        trim_trailing_whitespace(test_cases[i].input);
        assert_string_equal(test_cases[i].input, test_cases[i].expected);
    }
}

static void test_identify_disks_no_sys_class_nvme_present(void **state)
{
    (void)state; // Unused parameter

    char expected_string[1024];
    snprintf(expected_string, sizeof(expected_string), "no NVMe devices in %s: No such file or directory\n",
             fake_sys_class_nvme_path);
    remove_temp_dir(fake_sys_class_nvme_path);

    int result = identify_disks();
    assert_int_equal(result, 0);
    assert_string_equal(capture_stderr(), expected_string);
    assert_string_equal(capture_stdout(), "");
}

static void test_identify_disks(void **state)
{
    struct
    {
        const char *name;
        void (*setup_func)(void);
        const char *expected_stderr;
        const char *expected_stdout;
    } test_cases[] = {
        {"no namespaces", NULL, "", ""},
        {"nvme0", _setup_nvme0_microsoft_no_name_namespaces, "", ""},
        {"nvme1", _setup_nvme1_microsoft_one_namespace, "", "/dev/nvme1n1: key1=nvme1n1value1,key2=nvme1n1value2\n"},
        {"nvme2", _setup_nvme2_microsoft_two_namespaces, "",
         "/dev/nvme2n1: key1=nvme2n1value1,key2=nvme2n1value2\n"
         "/dev/nvme2n2: key1=nvme2n2value1,key2=nvme2n2value2\n"},
        {"nvme4", _setup_nvme4_non_microsoft, "", ""},
        {"nvme5", _setup_nvme5_microsoft_mixed_namespaces, "",
         "/dev/nvme5n1: key1=nvme5n1value1,key2=nvme5n1value2\n"
         "/dev/nvme5n2: \n"
         "/dev/nvme5n4: key1=nvme5n4value1,key2=nvme5n4value2\n"},
        {"nvme6", _setup_nvme6_remote_accelerator_v1_with_vs, "",
         "/dev/nvme6n1: key1=nvme6n1value1,key2=nvme6n1value2\n"},
        {"nvme7", _setup_nvme7_remote_accelerator_v1_without_vs, "",
         "/dev/nvme7n1: type=os\n"
         "/dev/nvme7n2: type=data,lun=0\n"
         "/dev/nvme7n3: type=data,lun=1\n"
         "/dev/nvme7n4: type=data,lun=2\n"
         "/dev/nvme7n9: type=data,lun=7\n"},
        {"nvme8", _setup_nvme8_direct_disk_v1_without_vs, "", "/dev/nvme8n1: type=local\n"},
        {"nvme9", _setup_nvme9_direct_disk_v2, "", "/dev/nvme9n1: key1=nvme9n1value1,key2=nvme9n1value2\n"},
        {"nvme10", _setup_nvme10_direct_disk_v2_missing_vs, "", "/dev/nvme10n1: type=local\n"}};

    for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++)
    {
        printf("Running test case: %s\n", test_cases[i].name);
        teardown(state);
        setup(state);

        if (test_cases[i].setup_func)
            test_cases[i].setup_func();

        int result = identify_disks();
        assert_int_equal(result, 0);
        assert_string_equal(capture_stderr(), test_cases[i].expected_stderr);
        assert_string_equal(capture_stdout(), test_cases[i].expected_stdout);
    }
}

static void test_identify_disks_combined(void **state)
{
    (void)state; // Unused parameter

    _setup_nvme0_microsoft_no_name_namespaces();
    _setup_nvme1_microsoft_one_namespace();
    _setup_nvme10_direct_disk_v2_missing_vs();
    _setup_nvme2_microsoft_two_namespaces();
    _setup_nvme4_non_microsoft();
    _setup_nvme5_microsoft_mixed_namespaces();
    _setup_nvme6_remote_accelerator_v1_with_vs();
    _setup_nvme7_remote_accelerator_v1_without_vs();
    _setup_nvme8_direct_disk_v1_without_vs();
    _setup_nvme9_direct_disk_v2();

    int result = identify_disks();

    assert_int_equal(result, 0);
    assert_string_equal(capture_stderr(), "");
    assert_string_equal(capture_stdout(), "/dev/nvme1n1: key1=nvme1n1value1,key2=nvme1n1value2\n"
                                          "/dev/nvme10n1: type=local\n"
                                          "/dev/nvme2n1: key1=nvme2n1value1,key2=nvme2n1value2\n"
                                          "/dev/nvme2n2: key1=nvme2n2value1,key2=nvme2n2value2\n"
                                          "/dev/nvme5n1: key1=nvme5n1value1,key2=nvme5n1value2\n"
                                          "/dev/nvme5n2: \n"
                                          "/dev/nvme5n4: key1=nvme5n4value1,key2=nvme5n4value2\n"
                                          "/dev/nvme6n1: key1=nvme6n1value1,key2=nvme6n1value2\n"
                                          "/dev/nvme7n1: type=os\n"
                                          "/dev/nvme7n2: type=data,lun=0\n"
                                          "/dev/nvme7n3: type=data,lun=1\n"
                                          "/dev/nvme7n4: type=data,lun=2\n"
                                          "/dev/nvme7n9: type=data,lun=7\n"
                                          "/dev/nvme8n1: type=local\n"
                                          "/dev/nvme9n1: key1=nvme9n1value1,key2=nvme9n1value2\n");
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_trim_trailing_whitespace),
        cmocka_unit_test_setup_teardown(test_identify_disks_no_sys_class_nvme_present, setup, teardown),
        cmocka_unit_test_setup_teardown(test_identify_disks, setup, teardown),
        cmocka_unit_test_setup_teardown(test_identify_disks_combined, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}

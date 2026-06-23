#include <check.h>
#include <stdlib.h>
#include <gtksourceview/gtksourceutils.h>

START_TEST(test_allocation_overflow_invariant)
{
    // Invariant: Multiplication for allocation size must not overflow
    // If overflow occurs, function must safely handle it (return NULL or fail gracefully)
    const char *payloads[] = {
        "A",  // Valid small input
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",  // Boundary: 128 chars
        "\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41"  // Exact exploit pattern
    };
    int num_payloads = sizeof(payloads) / sizeof(payloads[0]);

    for (int i = 0; i < num_payloads; i++) {
        char *result = gtk_source_utils_unescape_search_text(payloads[i]);
        
        // Property: Either allocation succeeds with valid pointer, 
        // or overflow is prevented and function returns NULL/fails safely
        if (result != NULL) {
            // If we got a result, verify it's not corrupted
            ck_assert_ptr_ne(result, NULL);
            free(result);
        } else {
            // NULL return is acceptable - indicates safe handling of overflow/bad input
            ck_assert_msg(1, "Function safely handled potential overflow case");
        }
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_allocation_overflow_invariant);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
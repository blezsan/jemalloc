#include "test/jemalloc_test.h"

#include "jemalloc/internal/flat_bitmap.h"
#include "test/nbits.h"

static void
do_test_init(size_t nbits) {
	size_t sz = FB_NGROUPS(nbits) * sizeof(fb_group_t);
	fb_group_t *fb = malloc(sz);
	/* Junk fb's contents. */
	memset(fb, 99, sz);
	fb_init(fb, nbits);
	for (size_t i = 0; i < nbits; i++) {
		expect_false(fb_get(fb, nbits, i),
		    "bitmap should start empty");
	}
	free(fb);
}

TEST_BEGIN(test_fb_init) {
#define NB(nbits) \
	do_test_init(nbits);
	NBITS_TAB
#undef NB
}
TEST_END

static void
do_test_get_set_unset(size_t nbits) {
	size_t sz = FB_NGROUPS(nbits) * sizeof(fb_group_t);
	fb_group_t *fb = malloc(sz);
	fb_init(fb, nbits);
	/* Set the bits divisible by 3. */
	for (size_t i = 0; i < nbits; i++) {
		if (i % 3 == 0) {
			fb_set(fb, nbits, i);
		}
	}
	/* Check them. */
	for (size_t i = 0; i < nbits; i++) {
		expect_b_eq(i % 3 == 0, fb_get(fb, nbits, i),
		    "Unexpected bit at position %zu", i);
	}
	/* Unset those divisible by 5. */
	for (size_t i = 0; i < nbits; i++) {
		if (i % 5 == 0) {
			fb_unset(fb, nbits, i);
		}
	}
	/* Check them. */
	for (size_t i = 0; i < nbits; i++) {
		expect_b_eq(i % 3 == 0 && i % 5 != 0, fb_get(fb, nbits, i),
		    "Unexpected bit at position %zu", i);
	}
	free(fb);
}

TEST_BEGIN(test_get_set_unset) {
#define NB(nbits) \
	do_test_get_set_unset(nbits);
	NBITS_TAB
#undef NB
}
TEST_END

static ssize_t
find_3_5_compute(ssize_t i, size_t nbits, bool bit, bool forward) {
	for(; i < (ssize_t)nbits && i >= 0; i += (forward ? 1 : -1)) {
		bool expected_bit = i % 3 == 0 || i % 5 == 0;
		if (expected_bit == bit) {
			return i;
		}
	}
	return forward ? (ssize_t)nbits : (ssize_t)-1;
}

static void
do_test_search_simple(size_t nbits) {
	size_t sz = FB_NGROUPS(nbits) * sizeof(fb_group_t);
	fb_group_t *fb = malloc(sz);
	fb_init(fb, nbits);

	/* We pick multiples of 3 or 5. */
	for (size_t i = 0; i < nbits; i++) {
		if (i % 3 == 0) {
			fb_set(fb, nbits, i);
		}
		/* This tests double-setting a little, too. */
		if (i % 5 == 0) {
			fb_set(fb, nbits, i);
		}
	}
	for (size_t i = 0; i < nbits; i++) {
		size_t ffs_compute = find_3_5_compute(i, nbits, true, true);
		size_t ffs_search = fb_ffs(fb, nbits, i);
		expect_zu_eq(ffs_compute, ffs_search, "ffs mismatch at %zu", i);

		ssize_t fls_compute = find_3_5_compute(i, nbits, true, false);
		size_t fls_search = fb_fls(fb, nbits, i);
		expect_zu_eq(fls_compute, fls_search, "fls mismatch at %zu", i);

		size_t ffu_compute = find_3_5_compute(i, nbits, false, true);
		size_t ffu_search = fb_ffu(fb, nbits, i);
		expect_zu_eq(ffu_compute, ffu_search, "ffu mismatch at %zu", i);

		size_t flu_compute = find_3_5_compute(i, nbits, false, false);
		size_t flu_search = fb_flu(fb, nbits, i);
		expect_zu_eq(flu_compute, flu_search, "flu mismatch at %zu", i);
	}

	free(fb);
}

TEST_BEGIN(test_search_simple) {
#define NB(nbits) \
	do_test_search_simple(nbits);
	NBITS_TAB
#undef NB
}
TEST_END

static void
expect_exhaustive_results(fb_group_t *mostly_full, fb_group_t *mostly_empty,
    size_t nbits, size_t special_bit, size_t position) {
	if (position < special_bit) {
		expect_zu_eq(special_bit, fb_ffs(mostly_empty, nbits, position),
		    "mismatch at %zu, %zu", position, special_bit);
		expect_zd_eq(-1, fb_fls(mostly_empty, nbits, position),
		    "mismatch at %zu, %zu", position, special_bit);
		expect_zu_eq(position, fb_ffu(mostly_empty, nbits, position),
		    "mismatch at %zu, %zu", position, special_bit);
		expect_zd_eq(position, fb_flu(mostly_empty, nbits, position),
		    "mismatch at %zu, %zu", position, special_bit);

		expect_zu_eq(position, fb_ffs(mostly_full, nbits, position),
		    "mismatch at %zu, %zu", position, special_bit);
		expect_zd_eq(position, fb_fls(mostly_full, nbits, position),
		    "mismatch at %zu, %zu", position, special_bit);
		expect_zu_eq(special_bit, fb_ffu(mostly_full, nbits, position),
		    "mismatch at %zu, %zu", position, special_bit);
		expect_zd_eq(-1, fb_flu(mostly_full, nbits, position),
		    "mismatch at %zu, %zu", position, special_bit);
	} else if (position == special_bit) {
		expect_zu_eq(special_bit, fb_ffs(mostly_empty, nbits, position),
		    "mismatch at %zu, %zu", position, special_bit);
		expect_zd_eq(special_bit, fb_fls(mostly_empty, nbits, position),
		    "mismatch at %zu, %zu", position, special_bit);
		expect_zu_eq(position + 1, fb_ffu(mostly_empty, nbits, position),
		    "mismatch at %zu, %zu", position, special_bit);
		expect_zd_eq(position - 1, fb_flu(mostly_empty, nbits,
		    position), "mismatch at %zu, %zu", position, special_bit);

		expect_zu_eq(position + 1, fb_ffs(mostly_full, nbits, position),
		    "mismatch at %zu, %zu", position, special_bit);
		expect_zd_eq(position - 1, fb_fls(mostly_full, nbits,
		    position), "mismatch at %zu, %zu", position, special_bit);
		expect_zu_eq(position, fb_ffu(mostly_full, nbits, position),
		    "mismatch at %zu, %zu", position, special_bit);
		expect_zd_eq(position, fb_flu(mostly_full, nbits, position),
		    "mismatch at %zu, %zu", position, special_bit);
	} else {
		/* position > special_bit. */
		expect_zu_eq(nbits, fb_ffs(mostly_empty, nbits, position),
		    "mismatch at %zu, %zu", position, special_bit);
		expect_zd_eq(special_bit, fb_fls(mostly_empty, nbits,
		    position), "mismatch at %zu, %zu", position, special_bit);
		expect_zu_eq(position, fb_ffu(mostly_empty, nbits, position),
		    "mismatch at %zu, %zu", position, special_bit);
		expect_zd_eq(position, fb_flu(mostly_empty, nbits, position),
		    "mismatch at %zu, %zu", position, special_bit);

		expect_zu_eq(position, fb_ffs(mostly_full, nbits, position),
		    "mismatch at %zu, %zu", position, special_bit);
		expect_zd_eq(position, fb_fls(mostly_full, nbits, position),
		    "mismatch at %zu, %zu", position, special_bit);
		expect_zu_eq(nbits, fb_ffu(mostly_full, nbits, position),
		    "mismatch at %zu, %zu", position, special_bit);
		expect_zd_eq(special_bit, fb_flu(mostly_full, nbits, position),
		    "mismatch at %zu, %zu", position, special_bit);
	}
}

static void
do_test_search_exhaustive(size_t nbits) {
	/* This test is quadratic; let's not get too big. */
	if (nbits > 1000) {
		return;
	}
	size_t sz = FB_NGROUPS(nbits) * sizeof(fb_group_t);
	fb_group_t *empty = malloc(sz);
	fb_init(empty, nbits);
	fb_group_t *full = malloc(sz);
	fb_init(full, nbits);
	fb_set_range(full, nbits, 0, nbits);

	for (size_t i = 0; i < nbits; i++) {
		fb_set(empty, nbits, i);
		fb_unset(full, nbits, i);

		for (size_t j = 0; j < nbits; j++) {
			expect_exhaustive_results(full, empty, nbits, i, j);
		}
		fb_unset(empty, nbits, i);
		fb_set(full, nbits, i);
	}

	free(empty);
	free(full);
}

TEST_BEGIN(test_search_exhaustive) {
#define NB(nbits) \
	do_test_search_exhaustive(nbits);
	NBITS_TAB
#undef NB
}
TEST_END

TEST_BEGIN(test_range_simple) {
	/*
	 * Just pick a constant big enough to have nontrivial middle sizes, and
	 * big enough that usages of things like weirdnum (below) near the
	 * beginning fit comfortably into the beginning of the bitmap.
	 */
	size_t nbits = 64 * 10;
	size_t ngroups = FB_NGROUPS(nbits);
	fb_group_t *fb = malloc(sizeof(fb_group_t) * ngroups);
	fb_init(fb, nbits);
	for (size_t i = 0; i < nbits; i++) {
		if (i % 2 == 0) {
			fb_set_range(fb, nbits, i, 1);
		}
	}
	for (size_t i = 0; i < nbits; i++) {
		expect_b_eq(i % 2 == 0, fb_get(fb, nbits, i),
		    "mismatch at position %zu", i);
	}
	fb_set_range(fb, nbits, 0, nbits / 2);
	fb_unset_range(fb, nbits, nbits / 2, nbits / 2);
	for (size_t i = 0; i < nbits; i++) {
		expect_b_eq(i < nbits / 2, fb_get(fb, nbits, i),
		    "mismatch at position %zu", i);
	}

	static const size_t weirdnum = 7;
	fb_set_range(fb, nbits, 0, nbits);
	fb_unset_range(fb, nbits, weirdnum, FB_GROUP_BITS + weirdnum);
	for (size_t i = 0; i < nbits; i++) {
		expect_b_eq(7 <= i && i <= 2 * weirdnum + FB_GROUP_BITS - 1,
		    !fb_get(fb, nbits, i), "mismatch at position %zu", i);
	}
	free(fb);
}
TEST_END

static void
do_test_empty_full_exhaustive(size_t nbits) {
	size_t sz = FB_NGROUPS(nbits) * sizeof(fb_group_t);
	fb_group_t *empty = malloc(sz);
	fb_init(empty, nbits);
	fb_group_t *full = malloc(sz);
	fb_init(full, nbits);
	fb_set_range(full, nbits, 0, nbits);

	expect_true(fb_full(full, nbits), "");
	expect_false(fb_empty(full, nbits), "");
	expect_false(fb_full(empty, nbits), "");
	expect_true(fb_empty(empty, nbits), "");

	for (size_t i = 0; i < nbits; i++) {
		fb_set(empty, nbits, i);
		fb_unset(full, nbits, i);

		expect_false(fb_empty(empty, nbits), "error at bit %zu", i);
		if (nbits != 1) {
			expect_false(fb_full(empty, nbits),
			    "error at bit %zu", i);
			expect_false(fb_empty(full, nbits),
			    "error at bit %zu", i);
		} else {
			expect_true(fb_full(empty, nbits),
			    "error at bit %zu", i);
			expect_true(fb_empty(full, nbits),
			    "error at bit %zu", i);
		}
		expect_false(fb_full(full, nbits), "error at bit %zu", i);

		fb_unset(empty, nbits, i);
		fb_set(full, nbits, i);
	}

	free(empty);
	free(full);
}

TEST_BEGIN(test_empty_full) {
#define NB(nbits) \
	do_test_empty_full_exhaustive(nbits);
	NBITS_TAB
#undef NB
}
TEST_END

TEST_BEGIN(test_iter_range_simple) {
	size_t set_limit = 30;
	size_t nbits = 100;
	fb_group_t fb[FB_NGROUPS(100)];

	fb_init(fb, nbits);

	/*
	 * Failing to initialize these can lead to build failures with -Wall;
	 * the compiler can't prove that they're set.
	 */
	size_t begin = (size_t)-1;
	size_t len = (size_t)-1;
	bool result;

	/* A set of checks with only the first set_limit bits *set*. */
	fb_set_range(fb, nbits, 0, set_limit);
	for (size_t i = 0; i < set_limit; i++) {
		result = fb_srange_iter(fb, nbits, i, &begin, &len);
		expect_true(result, "Should have found a range at %zu", i);
		expect_zu_eq(i, begin, "Incorrect begin at %zu", i);
		expect_zu_eq(set_limit - i, len, "Incorrect len at %zu", i);

		result = fb_urange_iter(fb, nbits, i, &begin, &len);
		expect_true(result, "Should have found a range at %zu", i);
		expect_zu_eq(set_limit, begin, "Incorrect begin at %zu", i);
		expect_zu_eq(nbits - set_limit, len, "Incorrect len at %zu", i);

		result = fb_srange_riter(fb, nbits, i, &begin, &len);
		expect_true(result, "Should have found a range at %zu", i);
		expect_zu_eq(0, begin, "Incorrect begin at %zu", i);
		expect_zu_eq(i + 1, len, "Incorrect len at %zu", i);

		result = fb_urange_riter(fb, nbits, i, &begin, &len);
		expect_false(result, "Should not have found a range at %zu", i);
	}
	for (size_t i = set_limit; i < nbits; i++) {
		result = fb_srange_iter(fb, nbits, i, &begin, &len);
		expect_false(result, "Should not have found a range at %zu", i);

		result = fb_urange_iter(fb, nbits, i, &begin, &len);
		expect_true(result, "Should have found a range at %zu", i);
		expect_zu_eq(i, begin, "Incorrect begin at %zu", i);
		expect_zu_eq(nbits - i, len, "Incorrect len at %zu", i);

		result = fb_srange_riter(fb, nbits, i, &begin, &len);
		expect_true(result, "Should have found a range at %zu", i);
		expect_zu_eq(0, begin, "Incorrect begin at %zu", i);
		expect_zu_eq(set_limit, len, "Incorrect len at %zu", i);

		result = fb_urange_riter(fb, nbits, i, &begin, &len);
		expect_true(result, "Should have found a range at %zu", i);
		expect_zu_eq(set_limit, begin, "Incorrect begin at %zu", i);
		expect_zu_eq(i - set_limit + 1, len, "Incorrect len at %zu", i);
	}

	/* A set of checks with only the first set_limit bits *unset*. */
	fb_unset_range(fb, nbits, 0, set_limit);
	fb_set_range(fb, nbits, set_limit, nbits - set_limit);
	for (size_t i = 0; i < set_limit; i++) {
		result = fb_srange_iter(fb, nbits, i, &begin, &len);
		expect_true(result, "Should have found a range at %zu", i);
		expect_zu_eq(set_limit, begin, "Incorrect begin at %zu", i);
		expect_zu_eq(nbits - set_limit, len, "Incorrect len at %zu", i);

		result = fb_urange_iter(fb, nbits, i, &begin, &len);
		expect_true(result, "Should have found a range at %zu", i);
		expect_zu_eq(i, begin, "Incorrect begin at %zu", i);
		expect_zu_eq(set_limit - i, len, "Incorrect len at %zu", i);

		result = fb_srange_riter(fb, nbits, i, &begin, &len);
		expect_false(result, "Should not have found a range at %zu", i);

		result = fb_urange_riter(fb, nbits, i, &begin, &len);
		expect_true(result, "Should not have found a range at %zu", i);
		expect_zu_eq(0, begin, "Incorrect begin at %zu", i);
		expect_zu_eq(i + 1, len, "Incorrect len at %zu", i);
	}
	for (size_t i = set_limit; i < nbits; i++) {
		result = fb_srange_iter(fb, nbits, i, &begin, &len);
		expect_true(result, "Should have found a range at %zu", i);
		expect_zu_eq(i, begin, "Incorrect begin at %zu", i);
		expect_zu_eq(nbits - i, len, "Incorrect len at %zu", i);

		result = fb_urange_iter(fb, nbits, i, &begin, &len);
		expect_false(result, "Should not have found a range at %zu", i);

		result = fb_srange_riter(fb, nbits, i, &begin, &len);
		expect_true(result, "Should have found a range at %zu", i);
		expect_zu_eq(set_limit, begin, "Incorrect begin at %zu", i);
		expect_zu_eq(i - set_limit + 1, len, "Incorrect len at %zu", i);

		result = fb_urange_riter(fb, nbits, i, &begin, &len);
		expect_true(result, "Should have found a range at %zu", i);
		expect_zu_eq(0, begin, "Incorrect begin at %zu", i);
		expect_zu_eq(set_limit, len, "Incorrect len at %zu", i);
	}

}
TEST_END

/*
 * Doing this bit-by-bit is too slow for a real implementation, but for testing
 * code, it's easy to get right.  In the exhaustive tests, we'll compare the
 * (fast but tricky) real implementation against the (slow but simple) testing
 * one.
 */
static bool
fb_iter_simple(fb_group_t *fb, size_t nbits, size_t start, size_t *r_begin,
    size_t *r_len, bool val, bool forward) {
	ssize_t stride = (forward ? (ssize_t)1 : (ssize_t)-1);
	ssize_t range_begin = (ssize_t)start;
	for (; range_begin != (ssize_t)nbits && range_begin != -1;
	    range_begin += stride) {
		if (fb_get(fb, nbits, range_begin) == val) {
			ssize_t range_end = range_begin;
			for (; range_end != (ssize_t)nbits && range_end != -1;
			    range_end += stride) {
				if (fb_get(fb, nbits, range_end) != val) {
					break;
				}
			}
			if (forward) {
				*r_begin = range_begin;
				*r_len = range_end - range_begin;
			} else {
				*r_begin = range_end + 1;
				*r_len = range_begin - range_end;
			}
			return true;
		}
	}
	return false;
}

static void
expect_iter_results_at(fb_group_t *fb, size_t nbits, size_t pos,
    bool val, bool forward) {
	bool iter_res;
	size_t iter_begin;
	size_t iter_len = 0;
	if (val) {
		if (forward) {
			iter_res = fb_srange_iter(fb, nbits, pos,
			    &iter_begin, &iter_len);
		} else {
			iter_res = fb_srange_riter(fb, nbits, pos,
			    &iter_begin, &iter_len);
		}
	} else {
		if (forward) {
			iter_res = fb_urange_iter(fb, nbits, pos,
			    &iter_begin, &iter_len);
		} else {
			iter_res = fb_urange_riter(fb, nbits, pos,
			    &iter_begin, &iter_len);
		}
	}

	bool simple_iter_res;
	size_t simple_iter_begin;
	size_t simple_iter_len = 0;
	simple_iter_res = fb_iter_simple(fb, nbits, pos, &simple_iter_begin,
	    &simple_iter_len, val, forward);

	expect_b_eq(iter_res, simple_iter_res, "Result mismatch at %zu", pos);
	if (iter_res && simple_iter_res) {
		assert_zu_eq(iter_begin, simple_iter_begin,
		    "Begin mismatch at %zu", pos);
		expect_zu_eq(iter_len, simple_iter_len,
		    "Length mismatch at %zu", pos);
	}
}

static void
expect_iter_results(fb_group_t *fb, size_t nbits) {
	for (size_t i = 0; i < nbits; i++) {
		expect_iter_results_at(fb, nbits, i, false, false);
		expect_iter_results_at(fb, nbits, i, false, true);
		expect_iter_results_at(fb, nbits, i, true, false);
		expect_iter_results_at(fb, nbits, i, true, true);
	}
}

static void
set_pattern_3(fb_group_t *fb, size_t nbits, bool zero_val) {
	for (size_t i = 0; i < nbits; i++) {
		if ((i % 6 < 3 && zero_val) || (i % 6 >= 3 && !zero_val)) {
			fb_set(fb, nbits, i);
		} else {
			fb_unset(fb, nbits, i);
		}
	}
}

static void
do_test_iter_range_exhaustive(size_t nbits) {
	/* This test is also pretty slow. */
	if (nbits > 1000) {
		return;
	}
	size_t sz = FB_NGROUPS(nbits) * sizeof(fb_group_t);
	fb_group_t *fb = malloc(sz);
	fb_init(fb, nbits);

	set_pattern_3(fb, nbits, /* zero_val */ true);
	expect_iter_results(fb, nbits);

	set_pattern_3(fb, nbits, /* zero_val */ false);
	expect_iter_results(fb, nbits);

	fb_set_range(fb, nbits, 0, nbits);
	fb_unset_range(fb, nbits, 0, nbits / 2 == 0 ? 1 : nbits / 2);
	expect_iter_results(fb, nbits);

	fb_unset_range(fb, nbits, 0, nbits);
	fb_set_range(fb, nbits, 0, nbits / 2 == 0 ? 1: nbits / 2);
	expect_iter_results(fb, nbits);

	free(fb);
}

TEST_BEGIN(test_iter_range_exhaustive) {
#define NB(nbits) \
	do_test_iter_range_exhaustive(nbits);
	NBITS_TAB
#undef NB
}
TEST_END

int
main(void) {
	return test_no_reentrancy(
	    test_fb_init,
	    test_get_set_unset,
	    test_search_simple,
	    test_search_exhaustive,
	    test_range_simple,
	    test_empty_full,
	    test_iter_range_simple,
	    test_iter_range_exhaustive);
}
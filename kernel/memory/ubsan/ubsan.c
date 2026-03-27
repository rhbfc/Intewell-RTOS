/*
 * Copyright (c) 2026 Kyland Inc.
 * Intewell-RTOS is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

#define KLOG_TAG "UBSAN"
#include <klog.h>

#include "ubsan.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define IS_ALIGNED(x, a) (((x) & ((a)-1)) == 0)

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const char *const g_type_check_kinds[] = { "load of",
                                                  "store to",
                                                  "reference binding to",
                                                  "member access within",
                                                  "member call on",
                                                  "constructor call on",
                                                  "downcast of",
                                                  "downcast of" };

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void ubsan_prologue (struct source_location *loc, const char *reason)
{
    KLOG_EMERG ("========================================"
                "========================================");
    KLOG_EMERG ("UBSAN: %s in %s:%" PRIu32 ":%" PRIu32 "", reason,
                loc->file_name, loc->line, loc->column);
}

static void ubsan_epilogue (void)
{
    KLOG_EMERG ("========================================"
                "========================================");
}

static void ubsan_prologue_epilogue (struct source_location *loc,
                                     const char             *reason)
{
    ubsan_prologue (loc, reason);
    ubsan_epilogue ();
}

static void handle_null_ptr_deref (struct type_mismatch_data_common *data)
{
    ubsan_prologue (data->location, "null-pointer-dereference");

    KLOG_EMERG ("%s null pointer of type %s",
                g_type_check_kinds[data->type_check_kind],
                data->type->type_name);

    ubsan_epilogue ();
}

static void handle_misaligned_access (struct type_mismatch_data_common *data,
                                      uintptr_t                         ptr)
{
    ubsan_prologue (data->location, "misaligned-access");

    KLOG_EMERG ("%s misaligned address %p for type %s",
                g_type_check_kinds[data->type_check_kind], (void *)ptr,
                data->type->type_name);
    KLOG_EMERG ("which requires %ld byte alignment", data->alignment);

    ubsan_epilogue ();
}

static void handle_object_size_mismatch (struct type_mismatch_data_common *data,
                                         uintptr_t                         ptr)
{
    ubsan_prologue (data->location, "object-size-mismatch");

    KLOG_EMERG ("%s address %p with insufficient space",
                g_type_check_kinds[data->type_check_kind], (void *)ptr);
    KLOG_EMERG ("for an object of type %s", data->type->type_name);

    ubsan_epilogue ();
}

static void ubsan_type_mismatch_common (struct type_mismatch_data_common *data,
                                        uintptr_t                         ptr)
{
    if (!ptr)
    {
        handle_null_ptr_deref (data);
    }
    else if (data->alignment && !IS_ALIGNED (ptr, data->alignment))
    {
        handle_misaligned_access (data, ptr);
    }
    else
    {
        handle_object_size_mismatch (data, ptr);
    }
}

static bool type_is_int (struct type_descriptor *type)
{
    return type->type_kind == TYPE_KIND_INT;
}

static bool type_is_signed (struct type_descriptor *type)
{
    return type->type_info & 1;
}

static unsigned type_bit_width (struct type_descriptor *type)
{
    return 1 << (type->type_info >> 1);
}

static bool is_inline_int (struct type_descriptor *type)
{
    unsigned inline_bits = sizeof (uintptr_t) * 8;
    unsigned bits        = type_bit_width (type);

    return bits <= inline_bits;
}

static int64_t get_signed_val (struct type_descriptor *type, void *val)
{
    if (is_inline_int (type))
    {
        unsigned bits = type_bit_width (type);
        uint64_t mask = (1llu << bits) - 1;
        uint64_t ret  = (uintptr_t)val & mask;

        return (int64_t)(((ret & (1llu << (bits - 1))) != 0) ? ret | ~mask
                                                             : ret);
    }

    return *(int64_t *)val;
}

static bool val_is_negative (struct type_descriptor *type, void *val)
{
    return type_is_signed (type) && get_signed_val (type, val) < 0;
}

static uint64_t get_unsigned_val (struct type_descriptor *type, void *val)
{
    if (is_inline_int (type))
    {
        return (uintptr_t)val;
    }

    return *(uint64_t *)val;
}

static void val_to_string (char *str, size_t size, struct type_descriptor *type,
                           void *value)
{
    if (type_is_int (type))
    {
        if (type_is_signed (type))
        {
            snprintf (str, size, "%" PRId64, get_signed_val (type, value));
        }
        else
        {
            snprintf (str, size, "%" PRIu64, get_unsigned_val (type, value));
        }
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void __ubsan_handle_out_of_bounds (void *data, void *index)
{
    struct out_of_bounds_data *info = data;
    char                       index_str[40];

    ubsan_prologue (&info->location, "array-index-out-of-bounds");

    val_to_string (index_str, sizeof (index_str), info->index_type, index);
    KLOG_EMERG ("index %s is out of range for type %s", index_str,
                info->array_type->type_name);
    ubsan_epilogue ();
}

void __ubsan_handle_shift_out_of_bounds (void *data, void *lhs, void *rhs)
{
    struct shift_out_of_bounds_data *info     = data;
    struct type_descriptor          *rhs_type = info->rhs_type;
    struct type_descriptor          *lhs_type = info->lhs_type;
    char                             rhs_str[40];
    char                             lhs_str[40];

    ubsan_prologue (&info->location, "shift-out-of-bounds");

    val_to_string (rhs_str, sizeof (rhs_str), rhs_type, rhs);
    val_to_string (lhs_str, sizeof (lhs_str), lhs_type, lhs);

    if (val_is_negative (rhs_type, rhs))
    {
        KLOG_EMERG ("shift exponent %s is negative", rhs_str);
    }
    else if (get_unsigned_val (rhs_type, rhs) >= type_bit_width (lhs_type))
    {
        KLOG_EMERG ("shift exponent %s is too large for %u-bit type %s",
                    rhs_str, type_bit_width (lhs_type), lhs_type->type_name);
    }
    else if (val_is_negative (lhs_type, lhs))
    {
        KLOG_EMERG ("left shift of negative value %s", lhs_str);
    }
    else
    {
        KLOG_EMERG ("left shift of %s by %s places cannot be"
                    " represented in type %s",
                    lhs_str, rhs_str, lhs_type->type_name);
    }

    ubsan_epilogue ();
}

void __ubsan_handle_divrem_overflow (void *data, void *lhs, void *rhs)
{
    struct overflow_data *info = data;
    char                  rhs_val_str[40];

    ubsan_prologue (&info->location, "division-overflow");

    val_to_string (rhs_val_str, sizeof (rhs_val_str), info->type, rhs);

    if (type_is_signed (info->type) && get_signed_val (info->type, rhs) == -1)
    {
        KLOG_EMERG ("division of %s by -1 cannot be represented in type %s",
                    rhs_val_str, info->type->type_name);
    }
    else
    {
        KLOG_EMERG ("division by zero");
    }
}

void __ubsan_handle_alignment_assumption (void *data, uintptr_t ptr,
                                          uintptr_t align, uintptr_t offset)
{
    struct alignment_assumption_data *info = data;
    uintptr_t                         real_ptr;

    ubsan_prologue (&info->location, "alignment-assumption");

    if (offset)
    {
        KLOG_EMERG (
            "assumption of %zu byte alignment (with offset of %zu byte) for"
            " pointer of type %s failed",
            align, offset, info->type->type_name);
    }
    else
    {
        KLOG_EMERG ("assumption of %zu byte alignment for pointer of type %s "
                    "failed",
                    align, info->type->type_name);
    }

    real_ptr = ptr - offset;
    KLOG_EMERG ("%saddress is %lu aligned, misalignment offset is %zu bytes",
                offset ? "offset " : "",
                1ul << (real_ptr ? __builtin_ffsl (real_ptr) : 0),
                real_ptr & (align - 1));

    ubsan_epilogue ();
}

void __ubsan_handle_type_mismatch (struct type_mismatch_data *data, void *ptr)
{
    struct type_mismatch_data_common common_data
        = { .location        = &data->location,
            .type            = data->type,
            .alignment       = data->alignment,
            .type_check_kind = data->type_check_kind };

    ubsan_type_mismatch_common (&common_data, (uintptr_t)ptr);
}

void __ubsan_handle_type_mismatch_v1 (void *_data, void *ptr)
{
    struct type_mismatch_data_v1    *data = _data;
    struct type_mismatch_data_common common_data
        = { .location        = &data->location,
            .type            = data->type,
            .alignment       = 1ul << data->log_alignment,
            .type_check_kind = data->type_check_kind };

    ubsan_type_mismatch_common (&common_data, (uintptr_t)ptr);
}

void __ubsan_handle_builtin_unreachable (void *data)
{
    ubsan_prologue_epilogue (data, "unreachable");
    while (1)
        ;
}

void __ubsan_handle_nonnull_arg (void *data)
{
    ubsan_prologue_epilogue (data, "nonnull-arg");
}

void __ubsan_handle_add_overflow (void *data, void *lhs, void *rhs)
{
    ubsan_prologue_epilogue (data, "add-overflow");
}

void __ubsan_handle_sub_overflow (void *data, void *lhs, void *rhs)
{
    ubsan_prologue_epilogue (data, "sub-overflow");
}

void __ubsan_handle_mul_overflow (void *data, void *lhs, void *rhs)
{
    ubsan_prologue_epilogue (data, "mul-overflow");
}

void __ubsan_handle_load_invalid_value (void *data, void *ptr)
{
    ubsan_prologue_epilogue (data, "load-invalid-value");
}

void __ubsan_handle_negate_overflow (void *data, void *ptr)
{
    ubsan_prologue_epilogue (data, "negate-overflow");
}

void __ubsan_handle_pointer_overflow (void *data, void *ptr, void *result)
{
    ubsan_prologue_epilogue (data, "pointer-overflow");
}

void __ubsan_handle_invalid_builtin (void *data)
{
    ubsan_prologue_epilogue (data, "invalid-builtin");
}

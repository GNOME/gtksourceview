#ifndef PCRE_MANGLED_H
#define PCRE_MANGLED_H

#define MANGLE_PCRE_PREFIX _gtk_source
#define MANGLE_PCRE(func) MANGLE_PCRE2(MANGLE_PCRE_PREFIX,func)
#define MANGLE_PCRE2(prefix,func) MANGLE_PCRE3(prefix,func)
#define MANGLE_PCRE3(prefix,func) prefix##_##func

#define _pcre_utf8_table1 MANGLE_PCRE(_pcre_utf8_table1)
#define _pcre_utf8_table2 MANGLE_PCRE(_pcre_utf8_table2)
#define _pcre_utf8_table3 MANGLE_PCRE(_pcre_utf8_table3)
#define _pcre_utf8_table4 MANGLE_PCRE(_pcre_utf8_table4)
#define _pcre_utf8_table1_size MANGLE_PCRE(_pcre_utf8_table1_size)
#define _pcre_utt MANGLE_PCRE(_pcre_utt)
#define _pcre_utt_size MANGLE_PCRE(_pcre_utt_size)
#define _pcre_default_tables MANGLE_PCRE(_pcre_default_tables)
#define _pcre_OP_lengths MANGLE_PCRE(_pcre_OP_lengths)

#define _pcre_is_newline MANGLE_PCRE(_pcre_is_newline)
#define _pcre_ord2utf8 MANGLE_PCRE(_pcre_ord2utf8)
#define _pcre_try_flipped MANGLE_PCRE(_pcre_try_flipped)
#define _pcre_ucp_findprop MANGLE_PCRE(_pcre_ucp_findprop)
#define _pcre_ucp_othercase MANGLE_PCRE(_pcre_ucp_othercase)
#define _pcre_valid_utf8 MANGLE_PCRE(_pcre_valid_utf8)
#define _pcre_was_newline MANGLE_PCRE(_pcre_was_newline)
#define _pcre_xclass MANGLE_PCRE(_pcre_xclass)

#define pcre_malloc MANGLE_PCRE(pcre_malloc)
#define pcre_free MANGLE_PCRE(pcre_free)
#define pcre_stack_malloc MANGLE_PCRE(pcre_stack_malloc)
#define pcre_stack_free MANGLE_PCRE(pcre_stack_free)
#define pcre_callout MANGLE_PCRE(pcre_callout)
#define pcre_compile MANGLE_PCRE(pcre_compile)
#define pcre_compile2 MANGLE_PCRE(pcre_compile2)
#define pcre_config MANGLE_PCRE(pcre_config)
#define pcre_copy_named_substring MANGLE_PCRE(pcre_copy_named_substring)
#define pcre_copy_substring MANGLE_PCRE(pcre_copy_substring)
#define pcre_dfa_exec MANGLE_PCRE(pcre_dfa_exec)
#define pcre_exec MANGLE_PCRE(pcre_exec)
#define pcre_free_substring MANGLE_PCRE(pcre_free_substring)
#define pcre_free_substring_list MANGLE_PCRE(pcre_free_substring_list)
#define pcre_fullinfo MANGLE_PCRE(pcre_fullinfo)
#define pcre_get_named_substring MANGLE_PCRE(pcre_get_named_substring)
#define pcre_get_stringnumber MANGLE_PCRE(pcre_get_stringnumber)
#define pcre_get_stringtable_entries MANGLE_PCRE(pcre_get_stringtable_entries)
#define pcre_get_substring MANGLE_PCRE(pcre_get_substring)
#define pcre_get_substring_list MANGLE_PCRE(pcre_get_substring_list)
#define pcre_info MANGLE_PCRE(pcre_info)
#define pcre_maketables MANGLE_PCRE(pcre_maketables)
#define pcre_refcount MANGLE_PCRE(pcre_refcount)
#define pcre_study MANGLE_PCRE(pcre_study)
#define pcre_version MANGLE_PCRE(pcre_version)

#include "pcre-real.h"

#endif /* PCRE_MANGLED_H */

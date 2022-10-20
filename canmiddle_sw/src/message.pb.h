/* Automatically generated nanopb header */
/* Generated by nanopb-0.4.6 */

#ifndef PB_MESSAGE_PB_H_INCLUDED
#define PB_MESSAGE_PB_H_INCLUDED
#include <pb.h>

#if PB_PROTO_HEADER_VERSION != 40
#error Regenerate this file with the current version of nanopb generator.
#endif

/* Struct definitions */
typedef PB_BYTES_ARRAY_T(8) CanMessage_value_t;
typedef struct _CanMessage { 
    bool has_prop;
    uint32_t prop;
    bool has_extended;
    bool extended;
    bool has_value;
    CanMessage_value_t value;
} CanMessage;


#ifdef __cplusplus
extern "C" {
#endif

/* Initializer values for message structs */
#define CanMessage_init_default                  {false, 0, false, 0, false, {0, {0}}}
#define CanMessage_init_zero                     {false, 0, false, 0, false, {0, {0}}}

/* Field tags (for use in manual encoding/decoding) */
#define CanMessage_prop_tag                      1
#define CanMessage_extended_tag                  2
#define CanMessage_value_tag                     3

/* Struct field encoding specification for nanopb */
#define CanMessage_FIELDLIST(X, a) \
X(a, STATIC,   OPTIONAL, UINT32,   prop,              1) \
X(a, STATIC,   OPTIONAL, BOOL,     extended,          2) \
X(a, STATIC,   OPTIONAL, BYTES,    value,             3)
#define CanMessage_CALLBACK NULL
#define CanMessage_DEFAULT NULL

extern const pb_msgdesc_t CanMessage_msg;

/* Defines for backwards compatibility with code written before nanopb-0.4.0 */
#define CanMessage_fields &CanMessage_msg

/* Maximum encoded size of messages (where known) */
#define CanMessage_size                          18

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif

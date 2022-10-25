/* Automatically generated nanopb header */
/* Generated by nanopb-0.4.6 */

#ifndef PB_MESSAGE_PB_H_INCLUDED
#define PB_MESSAGE_PB_H_INCLUDED
#include <pb.h>

#if PB_PROTO_HEADER_VERSION != 40
#error Regenerate this file with the current version of nanopb generator.
#endif

/* Enum definitions */
typedef enum _Metadata_Source { 
    Metadata_Source_MASTER = 1, 
    Metadata_Source_SLAVE = 2 
} Metadata_Source;

/* Struct definitions */
typedef PB_BYTES_ARRAY_T(40) CanMessage_value_t;
typedef struct _CanMessage { 
    bool has_prop;
    uint32_t prop;
    bool has_extended;
    bool extended;
    bool has_value;
    CanMessage_value_t value;
} CanMessage;

typedef struct _Metadata { 
    uint32_t recv_ms;
    Metadata_Source source;
} Metadata;

typedef struct _Request { 
    bool has_message_in;
    CanMessage message_in;
    /* Placeholder to avoid sending length 1 messages. */
    bool placeholder;
} Request;

typedef struct _Response { 
    pb_size_t messages_out_count;
    CanMessage messages_out[32];
    /* Placeholder to avoid sending length 0 messages. */
    bool placeholder;
    bool has_drops;
    uint32_t drops;
} Response;

typedef struct _SnoopData { 
    CanMessage message;
    Metadata metadata;
} SnoopData;


/* Helper constants for enums */
#define _Metadata_Source_MIN Metadata_Source_MASTER
#define _Metadata_Source_MAX Metadata_Source_SLAVE
#define _Metadata_Source_ARRAYSIZE ((Metadata_Source)(Metadata_Source_SLAVE+1))


#ifdef __cplusplus
extern "C" {
#endif

/* Initializer values for message structs */
#define CanMessage_init_default                  {false, 0, false, 0, false, {0, {0}}}
#define Request_init_default                     {false, CanMessage_init_default, 0}
#define Response_init_default                    {0, {CanMessage_init_default, CanMessage_init_default, CanMessage_init_default, CanMessage_init_default, CanMessage_init_default, CanMessage_init_default, CanMessage_init_default, CanMessage_init_default, CanMessage_init_default, CanMessage_init_default, CanMessage_init_default, CanMessage_init_default, CanMessage_init_default, CanMessage_init_default, CanMessage_init_default, CanMessage_init_default, CanMessage_init_default, CanMessage_init_default, CanMessage_init_default, CanMessage_init_default, CanMessage_init_default, CanMessage_init_default, CanMessage_init_default, CanMessage_init_default, CanMessage_init_default, CanMessage_init_default, CanMessage_init_default, CanMessage_init_default, CanMessage_init_default, CanMessage_init_default, CanMessage_init_default, CanMessage_init_default}, 0, false, 0}
#define Metadata_init_default                    {0, _Metadata_Source_MIN}
#define SnoopData_init_default                   {CanMessage_init_default, Metadata_init_default}
#define CanMessage_init_zero                     {false, 0, false, 0, false, {0, {0}}}
#define Request_init_zero                        {false, CanMessage_init_zero, 0}
#define Response_init_zero                       {0, {CanMessage_init_zero, CanMessage_init_zero, CanMessage_init_zero, CanMessage_init_zero, CanMessage_init_zero, CanMessage_init_zero, CanMessage_init_zero, CanMessage_init_zero, CanMessage_init_zero, CanMessage_init_zero, CanMessage_init_zero, CanMessage_init_zero, CanMessage_init_zero, CanMessage_init_zero, CanMessage_init_zero, CanMessage_init_zero, CanMessage_init_zero, CanMessage_init_zero, CanMessage_init_zero, CanMessage_init_zero, CanMessage_init_zero, CanMessage_init_zero, CanMessage_init_zero, CanMessage_init_zero, CanMessage_init_zero, CanMessage_init_zero, CanMessage_init_zero, CanMessage_init_zero, CanMessage_init_zero, CanMessage_init_zero, CanMessage_init_zero, CanMessage_init_zero}, 0, false, 0}
#define Metadata_init_zero                       {0, _Metadata_Source_MIN}
#define SnoopData_init_zero                      {CanMessage_init_zero, Metadata_init_zero}

/* Field tags (for use in manual encoding/decoding) */
#define CanMessage_prop_tag                      1
#define CanMessage_extended_tag                  2
#define CanMessage_value_tag                     3
#define Metadata_recv_ms_tag                     1
#define Metadata_source_tag                      2
#define Request_message_in_tag                   1
#define Request_placeholder_tag                  2
#define Response_messages_out_tag                1
#define Response_placeholder_tag                 2
#define Response_drops_tag                       3
#define SnoopData_message_tag                    1
#define SnoopData_metadata_tag                   2

/* Struct field encoding specification for nanopb */
#define CanMessage_FIELDLIST(X, a) \
X(a, STATIC,   OPTIONAL, UINT32,   prop,              1) \
X(a, STATIC,   OPTIONAL, BOOL,     extended,          2) \
X(a, STATIC,   OPTIONAL, BYTES,    value,             3)
#define CanMessage_CALLBACK NULL
#define CanMessage_DEFAULT NULL

#define Request_FIELDLIST(X, a) \
X(a, STATIC,   OPTIONAL, MESSAGE,  message_in,        1) \
X(a, STATIC,   REQUIRED, BOOL,     placeholder,       2)
#define Request_CALLBACK NULL
#define Request_DEFAULT NULL
#define Request_message_in_MSGTYPE CanMessage

#define Response_FIELDLIST(X, a) \
X(a, STATIC,   REPEATED, MESSAGE,  messages_out,      1) \
X(a, STATIC,   REQUIRED, BOOL,     placeholder,       2) \
X(a, STATIC,   OPTIONAL, UINT32,   drops,             3)
#define Response_CALLBACK NULL
#define Response_DEFAULT NULL
#define Response_messages_out_MSGTYPE CanMessage

#define Metadata_FIELDLIST(X, a) \
X(a, STATIC,   REQUIRED, UINT32,   recv_ms,           1) \
X(a, STATIC,   REQUIRED, UENUM,    source,            2)
#define Metadata_CALLBACK NULL
#define Metadata_DEFAULT (const pb_byte_t*)"\x10\x01\x00"

#define SnoopData_FIELDLIST(X, a) \
X(a, STATIC,   REQUIRED, MESSAGE,  message,           1) \
X(a, STATIC,   REQUIRED, MESSAGE,  metadata,          2)
#define SnoopData_CALLBACK NULL
#define SnoopData_DEFAULT NULL
#define SnoopData_message_MSGTYPE CanMessage
#define SnoopData_metadata_MSGTYPE Metadata

extern const pb_msgdesc_t CanMessage_msg;
extern const pb_msgdesc_t Request_msg;
extern const pb_msgdesc_t Response_msg;
extern const pb_msgdesc_t Metadata_msg;
extern const pb_msgdesc_t SnoopData_msg;

/* Defines for backwards compatibility with code written before nanopb-0.4.0 */
#define CanMessage_fields &CanMessage_msg
#define Request_fields &Request_msg
#define Response_fields &Response_msg
#define Metadata_fields &Metadata_msg
#define SnoopData_fields &SnoopData_msg

/* Maximum encoded size of messages (where known) */
#define CanMessage_size                          50
#define Metadata_size                            8
#define Request_size                             54
#define Response_size                            1672
#define SnoopData_size                           62

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif

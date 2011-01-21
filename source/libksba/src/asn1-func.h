/* asn1-func.h - definitions for asn1-func.c
 *      Copyright (C) 2000,2001 Fabio Fiorina
 *      Copyright (C) 2001 Free Software Foundation, Inc.
 *
 * This file is part of GNUTLS.
 *
 * GNUTLS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * GNUTLS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef ASN1_FUNC_H
#define ASN1_FUNC_H

typedef enum {
  TYPE_NONE = 0,
  TYPE_BOOLEAN = 1,
  TYPE_INTEGER = 2,
  TYPE_BIT_STRING = 3,
  TYPE_OCTET_STRING = 4,
  TYPE_NULL = 5,
  TYPE_OBJECT_ID = 6,
  TYPE_OBJECT_DESCRIPTOR = 7,
  TYPE_EXTERNAL = 8,
  TYPE_REAL = 9,
  TYPE_ENUMERATED = 10,
  TYPE_EMBEDDED_PDV = 11,
  TYPE_UTF8_STRING = 12,
  TYPE_REALTIVE_OID = 13,
  TYPE_SEQUENCE = 16,
  TYPE_SET = 17,
  TYPE_NUMERIC_STRING = 18,
  TYPE_PRINTABLE_STRING = 19,
  TYPE_TELETEX_STRING = 20,
  TYPE_VIDEOTEX_STRING = 21,
  TYPE_IA5_STRING = 22,
  TYPE_UTC_TIME = 23,
  TYPE_GENERALIZED_TIME = 24,
  TYPE_GRAPHIC_STRING = 25,
  TYPE_VISIBLE_STRING = 26,
  TYPE_GENERAL_STRING = 27,
  TYPE_UNIVERSAL_STRING = 28,
  TYPE_CHARACTER_STRING = 29,
  TYPE_BMP_STRING = 30,
  /* the following values do not correspond to an Universal tag */
  TYPE_CONSTANT = 128,
  TYPE_IDENTIFIER,
  TYPE_TAG,
  TYPE_DEFAULT,
  TYPE_SIZE,
  TYPE_SEQUENCE_OF,
  TYPE_ANY,
  TYPE_SET_OF,
  TYPE_DEFINITIONS,
  TYPE_CHOICE,
  TYPE_IMPORTS,
  TYPE_PRE_SEQUENCE  /* premanufactured Seqences as used by the DER encoder. */
} node_type_t;


enum tag_class {
  CLASS_UNIVERSAL = 0,
  CLASS_APPLICATION = 1,
  CLASS_CONTEXT = 2,
  CLASS_PRIVATE =3
};

/* Important: this must match the code in asn1-gentables.c */
struct node_flag_s {
  enum tag_class class;
  int explicit:1;
  int implicit:1;
  int has_imports:1;
  int assignment:1;  /* node is an assignment */
  int one_param:1;
  int has_tag:1; 
  int has_size:1;
  int has_list:1;
  int has_min_max:1;
  int has_defined_by:1;
  int is_false:1;
  int is_true:1;
  int has_default:1;  /* node has a default value (fixme:needed???)*/
  int is_optional:1;
  int is_implicit:1;
  int in_set:1;       
  int in_choice:1;
  int in_array:1;
  int is_any:1;      /* The der-encoder must change any to a real type
                        but still be aware that it actually is any */
  int not_used:1;     
  int help_down:1;    /* helper for create_tree */
  int help_right:1;   /* helper for create_tree */
  int tag_seen:1;
  int skip_this:1;   /* helper */
};

enum asn_value_type {
  VALTYPE_NULL = 0,
  VALTYPE_BOOL,
  VALTYPE_CSTR,
  VALTYPE_MEM,
  VALTYPE_LONG,
  VALTYPE_ULONG
};

union asn_value_u {
  int v_bool;
  char *v_cstr;
  struct {
    size_t len;
    unsigned char *buf;
  } v_mem;
  long v_long;
  unsigned long v_ulong;
};


/*
 * Structure definition used for the node of the tree that represents
 * an ASN.1 DEFINITION.
 */
#ifndef HAVE_TYPEDEFD_ASNNODE
typedef struct asn_node_struct *AsnNode; 
typedef struct asn_node_struct *asn_node_t; 
#define HAVE_TYPEDEFD_ASNNODE
#endif
struct asn_node_struct {
  char *name;                    /* Node name */
  node_type_t type;   
  struct node_flag_s flags;

  enum asn_value_type valuetype;
  union asn_value_u value;
  int off;                       /* offset of this TLV */
  int nhdr;                      /* length of the header */
  int len;                       /* length part of the TLV */
  node_type_t actual_type;       /* ugly helper to overcome TYPE_ANY probs*/

  AsnNode down;                  /* Pointer to the son node */
  AsnNode right;                 /* Pointer to the brother node */
  AsnNode left;                  /* Pointer to the next list element */ 
  AsnNode link_next;             /* to keep track of all nodes in a tree */
}; 

/* Structure to keep an entire ASN.1 parse tree and associated information */
struct ksba_asn_tree_s {
  AsnNode parse_tree;
  AsnNode node_list;  /* for easier release of all nodes */
  char filename[1];
};


typedef struct static_struct_asn {
  unsigned int name_off;        /* Node name */
  node_type_t type;             /* Node type */
  struct node_flag_s flags;
  unsigned int stringvalue_off;
} static_asn;


/*-- asn1-parse.y --*/
void _ksba_asn_release_nodes (AsnNode node);


/*-- asn1-func.c --*/
void _ksba_asn_set_value (AsnNode node, enum asn_value_type vtype,
                          const void *value, size_t len);
void _ksba_asn_set_name (AsnNode node, const char *name);
AsnNode _ksba_asn_walk_tree (AsnNode root, AsnNode node);
AsnNode _ksba_asn_walk_tree_up_right (AsnNode root, AsnNode node);
AsnNode _ksba_asn_find_node(AsnNode pointer,const char *name);
int _ksba_asn_check_identifier(AsnNode node);
int _ksba_asn_change_integer_value(AsnNode node);
int _ksba_asn_delete_not_used(AsnNode node);
int _ksba_asn_expand_object_id(AsnNode node);
void _ksba_asn_set_default_tag (AsnNode node);
void _ksba_asn_type_set_config (AsnNode node);
AsnNode _ksba_asn_expand_tree (AsnNode parse_tree, const char *name);
AsnNode _ksba_asn_insert_copy (AsnNode node);

int _ksba_asn_is_primitive (node_type_t type);
AsnNode _ksba_asn_new_node (node_type_t type);
void _ksba_asn_node_dump (AsnNode p, FILE *fp);
void _ksba_asn_node_dump_all (AsnNode root, FILE *fp);

AsnNode _ksba_asn_find_type_value (const unsigned char *image,
                                   AsnNode root, int idx,
                                   const void *oidbuf, size_t oidlen);


int _ksba_asn_delete_structure (AsnNode root);

/*-- asn2-func.c --*/
/*(functions are all declared in ksba.h)*/

/*-- asn1-tables.c (generated) --*/
const static_asn *_ksba_asn_lookup_table (const char *name,
                                          const char **stringtbl);



#endif /*ASN1_FUNC_H*/



/* asn1-func.c - Fucntions for the ASN.1 data structures.
 *      Copyright (C) 2000, 2001 Fabio Fiorina
 *      Copyright (C) 2001, 2010 Free Software Foundation, Inc.
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

#ifndef BUILD_GENTOOLS
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#ifdef BUILD_GENTOOLS
# include "gen-help.h"
#else
# include "util.h"
# include "ksba.h"
#endif

#include "asn1-func.h"


static AsnNode resolve_identifier (AsnNode root, AsnNode node, int nestlevel);


static AsnNode 
add_node (node_type_t type)
{
  AsnNode punt;

  punt = xmalloc (sizeof *punt);

  punt->left = NULL;
  punt->name = NULL;
  punt->type = type;
  punt->valuetype = VALTYPE_NULL;
  punt->value.v_cstr = NULL;
  punt->off = -1;
  punt->nhdr = 0;
  punt->len = 0;
  punt->down = NULL;
  punt->right = NULL;
  punt->link_next = NULL;
  return punt;
}

AsnNode
_ksba_asn_new_node (node_type_t type)
{
  return add_node (type);
}


int
_ksba_asn_is_primitive (node_type_t type)
{
  switch (type)
    {
    case TYPE_BOOLEAN:                               
    case TYPE_INTEGER:                               
    case TYPE_BIT_STRING:                            
    case TYPE_OCTET_STRING:                          
    case TYPE_NULL:                                  
    case TYPE_OBJECT_ID:                             
    case TYPE_OBJECT_DESCRIPTOR:                     
    case TYPE_REAL:                                  
    case TYPE_ENUMERATED:                            
    case TYPE_UTF8_STRING:                           
    case TYPE_REALTIVE_OID:                          
    case TYPE_NUMERIC_STRING:                        
    case TYPE_PRINTABLE_STRING:                      
    case TYPE_TELETEX_STRING:                        
    case TYPE_VIDEOTEX_STRING:                       
    case TYPE_IA5_STRING:                            
    case TYPE_UTC_TIME:                              
    case TYPE_GENERALIZED_TIME:                      
    case TYPE_GRAPHIC_STRING:                        
    case TYPE_VISIBLE_STRING:                        
    case TYPE_GENERAL_STRING:                        
    case TYPE_UNIVERSAL_STRING:                      
    case TYPE_CHARACTER_STRING:                      
    case TYPE_BMP_STRING:                            
    case TYPE_PRE_SEQUENCE:
      return 1;
    default:
      return 0;
    }
}


/* Change the value field of the node to the content of buffer value
   of size LEN.  With VALUE of NULL or LEN of 0 the value field is
   deleted */
void
_ksba_asn_set_value (AsnNode node,
                     enum asn_value_type vtype, const void *value, size_t len)
{
  return_if_fail (node);

  if (node->valuetype)
    {
      if (node->valuetype == VALTYPE_CSTR)
        xfree (node->value.v_cstr);
      else if (node->valuetype == VALTYPE_MEM)
        xfree (node->value.v_mem.buf);
      node->valuetype = 0;
    }

  switch (vtype)
    {
    case VALTYPE_NULL:
      break;
    case VALTYPE_BOOL:
      return_if_fail (len);
      node->value.v_bool = !!(const unsigned *)value;
      break;
    case VALTYPE_CSTR:
      node->value.v_cstr = xstrdup (value);
      break;
    case VALTYPE_MEM:
      node->value.v_mem.len = len;
      if (len)
        {
          node->value.v_mem.buf = xmalloc (len);
          memcpy (node->value.v_mem.buf, value, len);
        }
      else
          node->value.v_mem.buf = NULL;
      break;
    case VALTYPE_LONG:
      return_if_fail (sizeof (long) == len);
      node->value.v_long = *(long *)value;
      break;

    case VALTYPE_ULONG:
      return_if_fail (sizeof (unsigned long) == len);
      node->value.v_ulong = *(unsigned long *)value;
      break;

    default:
      return_if_fail (0);
    }
  node->valuetype = vtype;
}

static void
copy_value (AsnNode d, const AsnNode s)
{
  char helpbuf[1];
  const void *buf = NULL;
  size_t len = 0;

  return_if_fail (d != s);

  switch (s->valuetype)
    {
    case VALTYPE_NULL:
      break;
    case VALTYPE_BOOL:
      len = 1;
      helpbuf[0] = s->value.v_bool;
      buf = helpbuf;
      break;
    case VALTYPE_CSTR:
      buf = s->value.v_cstr;
      break;
    case VALTYPE_MEM:
      len = s->value.v_mem.len;
      buf = len? s->value.v_mem.buf : NULL;
      break;
    case VALTYPE_LONG:
      len = sizeof (long);
      buf = &s->value.v_long;
      break;
    case VALTYPE_ULONG:
      len = sizeof (unsigned long);
      buf = &s->value.v_ulong;
      break;

    default:
      return_if_fail (0);
    }
  _ksba_asn_set_value (d, s->valuetype, buf, len);
  d->off = s->off;
  d->nhdr = s->nhdr;
  d->len = s->len;
}

static AsnNode 
copy_node (const AsnNode s)
{
  AsnNode d = add_node (s->type);

  if (s->name)
    d->name = xstrdup (s->name);
  d->flags = s->flags;
  copy_value (d, s);
  return d;
}




/* Change the name field of the node to NAME.  
   NAME may be NULL */
void 
_ksba_asn_set_name (AsnNode node, const char *name)
{
  return_if_fail (node);

  if (node->name)
    {
      xfree (node->name);
      node->name = NULL;
    }

  if (name && *name)
      node->name = xstrdup (name);
}


static AsnNode 
set_right (AsnNode  node, AsnNode  right)
{
  if (node == NULL)
    return node;

  node->right = right;
  if (right)
    right->left = node;
  return node;
}


static AsnNode 
set_down (AsnNode node, AsnNode down)
{
  if (node == NULL)
    return node;

  node->down = down;
  if (down)
    down->left = node;
  return node;
}


void
_ksba_asn_remove_node (AsnNode  node)
{
  if (node == NULL)
    return;

  xfree (node->name);
  if (node->valuetype == VALTYPE_CSTR)
    xfree (node->value.v_cstr);
  else if (node->valuetype == VALTYPE_MEM)
    xfree (node->value.v_mem.buf);
  xfree (node);
}


/* find the node with the given name.  A name part of "?LAST" matches
   the last element of a set of */
static AsnNode 
find_node (AsnNode root, const char *name, int resolve)
{
  AsnNode p;
  const char *s;
  char buf[129];
  int i;

  if (!name || !name[0])
    return NULL;

  /* find the first part */
  s = name;
  for (i=0; *s && *s != '.' && i < DIM(buf)-1; s++)
    buf[i++] = *s;
  buf[i] = 0;
  return_null_if_fail (i < DIM(buf)-1);
          
  for (p = root; p && (!p->name || strcmp (p->name, buf)); p = p->right)
    ;

  /* find other parts */
  while (p && *s)
    {
      assert (*s == '.');
      s++; /* skip the dot */

      if (!p->down)
	return NULL; /* not found */
      p = p->down;

      for (i=0; *s && *s != '.' && i < DIM(buf)-1; s++)
        buf[i++] = *s;
      buf[i] = 0;
      return_null_if_fail (i < DIM(buf)-1);

      if (!*buf)
        {
         /* a double dot can be used to get over an unnamed sequence
            in a set - Actually a hack to workaround a bug.  We should
            rethink the entire node naming issue */
        }
      else if (!strcmp (buf, "?LAST"))
	{
	  if (!p)
	    return NULL;
	  while (p->right)
	    p = p->right;
	}
      else
	{
          for (; p ; p = p->right)
            {
              if (p->name && !strcmp (p->name, buf))
                break;
              if (resolve && p->name && p->type == TYPE_IDENTIFIER)
                {
                  AsnNode p2;
                  
                  p2 = resolve_identifier (root, p, 0);
                  if (p2 && p2->name && !strcmp (p2->name, buf))
                    break;
                }
            }
          
          if (resolve && p && p->type == TYPE_IDENTIFIER)
            p = resolve_identifier (root, p, 0);
	}
    }
  
  return p;
}

AsnNode 
_ksba_asn_find_node (AsnNode root, const char *name)
{
  return find_node (root, name, 0);
}


static AsnNode 
_asn1_find_left (AsnNode  node)
{
  if ((node == NULL) || (node->left == NULL) || (node->left->down == node))
    return NULL;

  return node->left;
}


static AsnNode 
find_up (AsnNode  node)
{
  AsnNode p;

  if (node == NULL)
    return NULL;

  p = node;
  while ((p->left != NULL) && (p->left->right == p))
    p = p->left;

  return p->left;
}



static void
print_value (AsnNode node, FILE *fp)
{
  if (!node->valuetype)
    return;
  fprintf (fp, " vt=%d val=", node->valuetype);
  switch (node->valuetype)
    {
    case VALTYPE_BOOL:
      fputs (node->value.v_bool? "True":"False", fp);
      break;
    case VALTYPE_CSTR:
      fputs (node->value.v_cstr, fp);
      break;
    case VALTYPE_MEM:
      {
        size_t n;
        unsigned char *p;
        for (p=node->value.v_mem.buf, n=node->value.v_mem.len; n; n--, p++)
          fprintf (fp, "%02X", *p);
      }
      break;
    case VALTYPE_LONG:
      fprintf (fp, "%ld", node->value.v_long);
      break;
    case VALTYPE_ULONG:
      fprintf (fp, "%lu", node->value.v_ulong);
      break;
    default:
      return_if_fail (0);
    }
}

void
_ksba_asn_node_dump (AsnNode p, FILE *fp)
{
  const char *typestr;

  switch (p->type)
    {
    case TYPE_NULL:	    typestr = "NULL"; break;
    case TYPE_CONSTANT:     typestr = "CONST"; break;
    case TYPE_IDENTIFIER:   typestr = "IDENTIFIER"; break;
    case TYPE_INTEGER:	    typestr = "INTEGER"; break;
    case TYPE_ENUMERATED:   typestr = "ENUMERATED"; break;
    case TYPE_UTC_TIME:	    typestr = "UTCTIME"; break;
    case TYPE_GENERALIZED_TIME: typestr = "GENERALIZEDTIME"; break;
    case TYPE_BOOLEAN:	    typestr = "BOOLEAN"; break;
    case TYPE_SEQUENCE:	    typestr = "SEQUENCE"; break;
    case TYPE_PRE_SEQUENCE: typestr = "PRE_SEQUENCE"; break;
    case TYPE_BIT_STRING:   typestr = "BIT_STR"; break;
    case TYPE_OCTET_STRING: typestr = "OCT_STR"; break;
    case TYPE_TAG:	    typestr = "TAG"; break;
    case TYPE_DEFAULT:	    typestr = "DEFAULT"; break;
    case TYPE_SIZE:	    typestr = "SIZE"; break;
    case TYPE_SEQUENCE_OF:  typestr = "SEQ_OF"; break;
    case TYPE_OBJECT_ID:    typestr = "OBJ_ID"; break;
    case TYPE_ANY:	    typestr = "ANY"; break;
    case TYPE_SET:          typestr = "SET"; break;
    case TYPE_SET_OF: 	    typestr = "SET_OF"; break;
    case TYPE_CHOICE:	    typestr = "CHOICE"; break;
    case TYPE_DEFINITIONS:  typestr = "DEFINITIONS"; break;
    case TYPE_UTF8_STRING:       typestr = "UTF8_STRING"; break;
    case TYPE_NUMERIC_STRING:    typestr = "NUMERIC_STRING"; break;
    case TYPE_PRINTABLE_STRING:  typestr = "PRINTABLE_STRING"; break;
    case TYPE_TELETEX_STRING:    typestr = "TELETEX_STRING"; break; 
    case TYPE_IA5_STRING:        typestr = "IA5_STRING"; break;
    default:	            typestr = "ERROR\n"; break;
    }

  fprintf (fp, "%s", typestr);
  if (p->name)
    fprintf (fp, " `%s'", p->name);
  print_value (p, fp);
  fputs ("  ", fp);
  switch (p->flags.class)
    { 
    case CLASS_UNIVERSAL:   fputs ("U", fp); break;
    case CLASS_PRIVATE:     fputs ("P", fp); break;
    case CLASS_APPLICATION: fputs ("A", fp); break;
    case CLASS_CONTEXT:     fputs ("C", fp); break;
    }
  
  if (p->flags.explicit)
    fputs (",explicit", fp);
  if (p->flags.implicit)
    fputs (",implicit", fp);
  if (p->flags.is_implicit)
    fputs (",is_implicit", fp);
  if (p->flags.has_tag)
    fputs (",tag", fp);
  if (p->flags.has_default)
    fputs (",default", fp);
  if (p->flags.is_true)
    fputs (",true", fp);
  if (p->flags.is_false)
    fputs (",false", fp);
  if (p->flags.has_list)
    fputs (",list", fp);
  if (p->flags.has_min_max)
    fputs (",min_max", fp);
  if (p->flags.is_optional)
    fputs (",optional", fp);
  if (p->flags.one_param)
    fputs (",1_param", fp);
  if (p->flags.has_size)
    fputs (",size", fp);
  if (p->flags.has_defined_by)
    fputs (",def_by", fp);
  if (p->flags.has_imports)
    fputs (",imports", fp);
  if (p->flags.assignment)
    fputs (",assign",fp);
  if (p->flags.in_set)
    fputs (",in_set",fp);
  if (p->flags.in_choice)
    fputs (",in_choice",fp);
  if (p->flags.in_array)
    fputs (",in_array",fp);
  if (p->flags.not_used)
    fputs (",not_used",fp);
  if (p->flags.skip_this)
    fputs (",[skip]",fp);
  if (p->flags.is_any)
    fputs (",is_any",fp);
  if (p->off != -1 )
    fprintf (fp, " %d.%d.%d", p->off, p->nhdr, p->len );
  
}

void
_ksba_asn_node_dump_all (AsnNode root, FILE *fp)
{
  AsnNode p = root;
  int indent = 0;

  while (p)
    {
      fprintf (fp, "%*s", indent, "");
      _ksba_asn_node_dump (p, fp);
      putc ('\n', fp);

      if (p->down)
	{
	  p = p->down;
	  indent += 2;
	}
      else if (p == root)
	{
	  p = NULL;
	  break;
	}
      else if (p->right)
	p = p->right;
      else
	{
	  while (1)
	    {
	      p = find_up (p);
	      if (p == root)
		{
		  p = NULL;
		  break;
		}
	      indent -= 2;
	      if (p->right)
		{
		  p = p->right;
		  break;
		}
	    }
	}
    }
}

/**
 * ksba_asn_tree_dump:
 * @tree: A Parse Tree
 * @name: Name of the element or NULL
 * @fp: dump to this stream
 *
 * If the first character of the name is a '<' the expanded version of
 * the tree will be printed.
 * 
 * This function is a debugging aid.
 **/
void
ksba_asn_tree_dump (ksba_asn_tree_t tree, const char *name, FILE *fp)
{
  AsnNode p, root;
  int k, expand=0, indent = 0;

  if (!tree || !tree->parse_tree)
    return;

  if ( name && *name== '<')
    {
      expand = 1;
      name++;
      if (!*name)
        name = NULL;
    }

  root = name? _ksba_asn_find_node (tree->parse_tree, name) : tree->parse_tree;
  if (!root)
    return;

  if (expand)
    root = _ksba_asn_expand_tree (root, NULL);

  p = root;
  while (p)
    {
      for (k = 0; k < indent; k++)
	fprintf (fp, " ");
      _ksba_asn_node_dump (p, fp);
      putc ('\n', fp);

      if (p->down)
	{
	  p = p->down;
	  indent += 2;
	}
      else if (p == root)
	{
	  p = NULL;
	  break;
	}
      else if (p->right)
	p = p->right;
      else
	{
	  while (1)
	    {
	      p = find_up (p);
	      if (p == root)
		{
		  p = NULL;
		  break;
		}
	      indent -= 2;
	      if (p->right)
		{
		  p = p->right;
		  break;
		}
	    }
	}
    }

  if (expand)
    _ksba_asn_release_nodes (root);
}

int
_ksba_asn_delete_structure (AsnNode root)
{
  AsnNode p, p2, p3;

  if (root == NULL)
    return gpg_error (GPG_ERR_ELEMENT_NOT_FOUND);

  p = root;
  while (p)
    {
      if (p->down)
	{
	  p = p->down;
	}
      else
	{			/* no down */
	  p2 = p->right;
	  if (p != root)
	    {
	      p3 = find_up (p);
	      set_down (p3, p2);
	      _ksba_asn_remove_node (p);
	      p = p3;
	    }
	  else
	    {			/* p==root */
	      p3 = _asn1_find_left (p);
	      if (!p3)
		{
		  p3 = find_up (p);
		  if (p3)
		    set_down (p3, p2);
		  else
		    {
		      if (p->right)
			p->right->left = NULL;
		    }
		}
	      else
		set_right (p3, p2);
	      _ksba_asn_remove_node (p);
	      p = NULL;
	    }
	}
    }
  return 0;
}


/* check that all identifiers referenced in the tree are available */
int
_ksba_asn_check_identifier (AsnNode node)
{
  AsnNode p, p2;
  char name2[129];

  if (!node)
    return gpg_error (GPG_ERR_ELEMENT_NOT_FOUND);

  for (p = node; p; p = _ksba_asn_walk_tree (node, p))
    {
      if (p->type == TYPE_IDENTIFIER && p->valuetype == VALTYPE_CSTR)
	{
          if (strlen (node->name)+strlen(p->value.v_cstr)+2 > DIM(name2))
            return gpg_error (GPG_ERR_BUG); /* well identifier too long */
          strcpy (name2, node->name); 
          strcat (name2, ".");
	  strcat (name2, p->value.v_cstr);
	  p2 = _ksba_asn_find_node (node, name2);
	  if (!p2)
	    {
	      fprintf (stderr,"reference to `%s' not found\n", name2);
	      return gpg_error (GPG_ERR_IDENTIFIER_NOT_FOUND);
	    }
/*            fprintf (stdout,"found reference for `%s' (", name2); */
/*            print_node (p2, stdout); */
/*            fputs (")\n", stdout); */
	}
      else if (p->type == TYPE_OBJECT_ID && p->flags.assignment)
	{ /* an object ID in an assignment */
	  p2 = p->down;
	  if (p2 && (p2->type == TYPE_CONSTANT))
            {  
	      if (p2->valuetype == VALTYPE_CSTR && !isdigit (p2->value.v_cstr[0]))
		{ /* the first constand below is a reference */
                  if (strlen (node->name)
                      +strlen(p->value.v_cstr)+2 > DIM(name2))
                    return gpg_error (GPG_ERR_BUG); /* well identifier too long */
                  strcpy (name2, node->name);
                  strcat (name2, ".");
		  strcat (name2, p2->value.v_cstr);
		  p2 = _ksba_asn_find_node (node, name2);
		  if (!p2)
                    {
                      fprintf (stderr,"object id reference `%s' not found\n",
                               name2);
                      return gpg_error (GPG_ERR_IDENTIFIER_NOT_FOUND);
                    }
                  else if ( p2->type != TYPE_OBJECT_ID 
                            || !p2->flags.assignment )
		    {
		      fprintf (stderr,"`%s' is not an object id\n", name2);
		      return gpg_error (GPG_ERR_IDENTIFIER_NOT_FOUND);
		    }
/*                    fprintf (stdout,"found objid reference for `%s' (", name2); */
/*                    print_node (p2, stdout); */
/*                    fputs (")\n", stdout); */
		}
	    }
	}
    }

  return 0;
}


/* Get the next node until root is reached in which case NULL is
   returned */
AsnNode
_ksba_asn_walk_tree (AsnNode root, AsnNode node)
{
  if (!node)
    ;
  else if (node->down)
    node = node->down;
  else
    {
      if (node == root)
        node = NULL;
      else if (node->right)
        node = node->right;
      else
        {
          for (;;)
            {
              node = find_up (node);
              if (node == root)
                {
                  node = NULL;
                  break;
                }
              if (node->right)
                {
                  node = node->right;
                  break;
                }
            }
        }
    }

  return node;
}

AsnNode
_ksba_asn_walk_tree_up_right (AsnNode root, AsnNode node)
{
  if (node)
    {
      if (node == root)
        node = NULL;
      else
        {
          for (;;)
            {
              node = find_up (node);
              if (node == root)
                {
                  node = NULL;
                  break;
                }
              if (node->right)
                {
                  node = node->right;
                  break;
                }
            }
        }
    }

  return node;
}

/* walk over the tree and change the value type of all integer types
   from string to long. */
int
_ksba_asn_change_integer_value (AsnNode node)
{
  AsnNode p;

  if (node == NULL)
    return gpg_error (GPG_ERR_ELEMENT_NOT_FOUND);

  for (p = node; p; p = _ksba_asn_walk_tree (node, p))
    {
      if (p->type == TYPE_INTEGER && p->flags.assignment)
	{
	  if (p->valuetype == VALTYPE_CSTR)
	    {
              long val = strtol (p->value.v_cstr, NULL, 10);
	      _ksba_asn_set_value (p, VALTYPE_LONG, &val, sizeof(val));
	    }
	}
    }

  return 0;
}



/* Expand all object ID constants */
int
_ksba_asn_expand_object_id (AsnNode node)
{
  AsnNode p, p2, p3, p4, p5;
  char name_root[129], name2[129*2+1];

  /* Fixme: Make a cleaner implementation */
  if (!node)
    return gpg_error (GPG_ERR_ELEMENT_NOT_FOUND);
  if (!node->name)
    return gpg_error (GPG_ERR_INV_VALUE);
  if (strlen(node->name) >= DIM(name_root)-1)
    return gpg_error (GPG_ERR_GENERAL);
  strcpy (name_root, node->name);

 restart:
  for (p = node; p; p = _ksba_asn_walk_tree (node, p))
    {
      if (p->type == TYPE_OBJECT_ID && p->flags.assignment)
        {
          p2 = p->down;
          if (p2 && p2->type == TYPE_CONSTANT)
            {
              if (p2->valuetype == VALTYPE_CSTR
                  && !isdigit (p2->value.v_cstr[0]))
                {
                  if (strlen(p2->value.v_cstr)+1+strlen(name2) >= DIM(name2)-1)
                    return gpg_error (GPG_ERR_GENERAL);
                  strcpy (name2, name_root);
                  strcat (name2, ".");
                  strcat (name2, p2->value.v_cstr);
                  p3 = _ksba_asn_find_node (node, name2);
                  if (!p3 || p3->type != TYPE_OBJECT_ID ||
                      !p3->flags.assignment)
                    return gpg_error (GPG_ERR_ELEMENT_NOT_FOUND);
                  set_down (p, p2->right);
                  _ksba_asn_remove_node (p2);
                  p2 = p;
                  p4 = p3->down;
                  while (p4)
                    {
                      if (p4->type == TYPE_CONSTANT)
                        {
                          p5 = add_node (TYPE_CONSTANT);
                          _ksba_asn_set_name (p5, p4->name);
                          _ksba_asn_set_value (p5, VALTYPE_CSTR,
                                               p4->value.v_cstr, 0);
                          if (p2 == p)
                            {
                              set_right (p5, p->down);
                              set_down (p, p5);
                            }
                          else
                            {
                              set_right (p5, p2->right);
                              set_right (p2, p5);
                            }
                          p2 = p5;
                        }
                      p4 = p4->right;
                    }
                  goto restart;  /* the most simple way to get it right ;-) */
                }
            }
        }
    }
  return 0;
}

/* Walk the parse tree and set the default tag where appropriate.  The
   node must be of type DEFINITIONS */
void
_ksba_asn_set_default_tag (AsnNode node)
{
  AsnNode p;

  return_if_fail (node && node->type == TYPE_DEFINITIONS);

  for (p = node; p; p = _ksba_asn_walk_tree (node, p))
    {
      if ( p->type == TYPE_TAG
           && !p->flags.explicit && !p->flags.implicit)
	{
	  if (node->flags.explicit)
	    p->flags.explicit = 1;
	  else
	    p->flags.implicit = 1;
	}
    }
  /* now mark the nodes which are implicit */
  for (p = node; p; p = _ksba_asn_walk_tree (node, p))
    {
      if ( p->type == TYPE_TAG && p->flags.implicit && p->down)
	{
	  if (p->down->type == TYPE_CHOICE)
            ; /* a CHOICE is per se implicit */
	  else if (p->down->type != TYPE_TAG)
	    p->down->flags.is_implicit = 1;
	}
    }
}

/* Walk the tree and set the is_set and not_used flags for all nodes below
   a node of type SET. */
void
_ksba_asn_type_set_config (AsnNode node)
{
  AsnNode p, p2;

  return_if_fail (node && node->type == TYPE_DEFINITIONS);

  for (p = node; p; p = _ksba_asn_walk_tree (node, p))
    {
      if (p->type == TYPE_SET)
        {
          for (p2 = p->down; p2; p2 = p2->right)
            {
              if (p2->type != TYPE_TAG)
                {
                  p2->flags.in_set = 1;
                  p2->flags.not_used = 1;
                }
            }
        }
      else if (p->type == TYPE_CHOICE)
        {
          for (p2 = p->down; p2; p2 = p2->right)
            {
                p2->flags.in_choice = 1;
            }
        }
      else if (p->type == TYPE_SEQUENCE_OF || p->type == TYPE_SET_OF)
        {
          for (p2 = p->down; p2; p2 = p2->right)
            p2->flags.in_array = 1;
        }
      else if (p->type == TYPE_ANY)
        { /* Help the DER encoder to track ANY tags */
          p->flags.is_any = 1;
        }
    }
}

/* Create a copy the tree at SRC_ROOT. s is a helper which should be
   set to SRC_ROOT by the caller */
static AsnNode
copy_tree (AsnNode src_root, AsnNode s)
{
  AsnNode first=NULL, dprev=NULL, d, down, tmp;
  AsnNode *link_nextp = NULL;

  for (; s; s=s->right )
    {
      down = s->down;
      d = copy_node (s);
      if (link_nextp)
	*link_nextp = d;
      link_nextp = &d->link_next;
      
      if (!first)
        first = d;
      else
        {
          dprev->right = d;
          d->left = dprev;
        }
      dprev = d;
      if (down)
        {
          tmp = copy_tree (src_root, down);
	  if (tmp)
	    {
	      if (link_nextp)
		*link_nextp = tmp;
	      link_nextp = &tmp->link_next;
	      while (*link_nextp)
		link_nextp = &(*link_nextp)->link_next;
	    }
	      
          if (d->down && tmp)
            { /* Need to merge it with the existing down */
              AsnNode x;

              for (x=d->down; x->right; x = x->right)
                ;
              x->right = tmp;
              tmp->left = x;
            }
          else 
            {
              d->down = tmp;
              if (d->down)
                d->down->left = d;
            }
        }
    }
  return first;
}



static AsnNode
resolve_identifier (AsnNode root, AsnNode node, int nestlevel)
{
  char buf_space[50];
  char *buf;
  AsnNode n;
  size_t bufsize;

  if (nestlevel > 20)
    return NULL;

  return_null_if_fail (root);
  return_null_if_fail (node->valuetype == VALTYPE_CSTR);

  bufsize = strlen (root->name) + strlen (node->value.v_cstr) + 2;
  if (bufsize <= sizeof buf_space)
    buf = buf_space;
  else
    {
      buf = xtrymalloc (bufsize);
      return_null_if_fail (buf);
    }
  strcpy (stpcpy (stpcpy (buf, root->name), "."), node->value.v_cstr);
  n = _ksba_asn_find_node (root, buf);

  /* We do just a simple indirection. */
  if (n && n->type == TYPE_IDENTIFIER)
    n = resolve_identifier (root, n, nestlevel+1);

  if (buf != buf_space)
    xfree (buf);

  return n;
}


static AsnNode
do_expand_tree (AsnNode src_root, AsnNode s, int depth)
{
  AsnNode first=NULL, dprev=NULL, d, down, tmp;
  AsnNode *link_nextp = NULL;

  /* On the very first level we do not follow the right pointer so that
     we can break out a valid subtree. */
  for (; s; s=depth?s->right:NULL )
    {
      if (s->type == TYPE_SIZE)
        continue; /* this node gets in the way all the time.  It
                     should be an attribute to a node */
      
      down = s->down;
      if (s->type == TYPE_IDENTIFIER)
        {
          AsnNode s2, *dp;

          d = resolve_identifier (src_root, s, 0);
          if (!d)
            {
              fprintf (stderr, "RESOLVING IDENTIFIER FAILED\n");
              continue;
            }
          down = d->down;
          d = copy_node (d);
	  if (link_nextp)
	    *link_nextp = d;
	  link_nextp = &d->link_next;
          if (s->flags.is_optional)
            d->flags.is_optional = 1;
          if (s->flags.in_choice)
            d->flags.in_choice = 1;
          if (s->flags.in_array)
            d->flags.in_array = 1;
          if (s->flags.is_implicit)
            d->flags.is_implicit = 1;
          if (s->flags.is_any)
            d->flags.is_any = 1;
          /* we don't want the resolved name - change it back */
          _ksba_asn_set_name (d, s->name);
          /* copy the default and tag attributes */
          tmp = NULL;
          dp = &tmp;
          for (s2=s->down; s2; s2=s2->right)
            {
              AsnNode x;

              x = copy_node (s2);
	      if (link_nextp)
		*link_nextp = x;
	      link_nextp = &x->link_next;
              x->left = *dp? *dp : d;
              *dp = x;
              dp = &(*dp)->right;

              if (x->type == TYPE_TAG)
                d->flags.has_tag =1;
              else if (x->type == TYPE_DEFAULT)
                d->flags.has_default =1;
            }
          d->down = tmp;
        }
      else
        {
	  d = copy_node (s);
	  if (link_nextp)
	    *link_nextp = d;
	  link_nextp = &d->link_next;
	}

      if (!first)
        first = d;
      else
        {
          dprev->right = d;
          d->left = dprev;
        }
      dprev = d;
      if (down)
        {
          if (depth >= 1000)
            {
              fprintf (stderr, "ASN.1 TREE TOO TALL!\n");
              tmp = NULL;
            }
          else
            {
	      tmp = do_expand_tree (src_root, down, depth+1);
	      if (tmp)
		{
		  if (link_nextp)
		    *link_nextp = tmp;
		  link_nextp = &tmp->link_next;
		  while (*link_nextp)
		    link_nextp = &(*link_nextp)->link_next;
		}
	    }
          if (d->down && tmp)
            { /* Need to merge it with the existing down */
              AsnNode x;

              for (x=d->down; x->right; x = x->right)
                ;
              x->right = tmp;
              tmp->left = x;
            }
          else 
            {
              d->down = tmp;
              if (d->down)
                d->down->left = d;
            }
        }
    }

  return first;
}

  
/* Expand the syntax tree so that all references are resolved and we
   are able to store values right in the tree (except for set/sequence
   of).  This expanded tree is also an requirement for doing the DER
   decoding as the resolving of identifiers leads to a lot of
   problems.  We use more memory of course, but this is negligible
   because the entire code will be simpler and faster */
AsnNode
_ksba_asn_expand_tree (AsnNode parse_tree, const char *name)
{
  AsnNode root;

  root = name? find_node (parse_tree, name, 1) : parse_tree;
  return do_expand_tree (parse_tree, root, 0);
}


/* Insert a copy of the entire tree at NODE as the sibling of itself
   and return the copy */
AsnNode
_ksba_asn_insert_copy (AsnNode node)
{
  AsnNode n;
  AsnNode *link_nextp;

  n = copy_tree (node, node);
  if (!n)
    return NULL; /* out of core */
  return_null_if_fail (n->right == node->right);
  node->right = n;
  n->left = node;

  /* FIXME: Consider tail pointer for faster insertion.  */
  link_nextp = &node->link_next;
  while (*link_nextp)
    link_nextp = &(*link_nextp)->link_next;
  *link_nextp = n;

  return n;
}


/* Locate a type value sequence like

  SEQUENCE { 
     type    OBJECT IDENTIFIER
     value   ANY
  }

  below root and return the 'value' node.  OIDBUF should contain the
  DER encoding of an OID value.  idx is the number of OIDs to skip;
  this can be used to enumerate structures with the same OID */
AsnNode
_ksba_asn_find_type_value (const unsigned char *image, AsnNode root, int idx,
                           const void *oidbuf, size_t oidlen)
{
  AsnNode n, noid;

  if (!image || !root)
    return NULL;

  for (n = root; n; n = _ksba_asn_walk_tree (root, n) )
    {
      if ( n->type == TYPE_SEQUENCE
           && n->down && n->down->type == TYPE_OBJECT_ID)
        {
          noid = n->down;
          if (noid->off != -1 && noid->len == oidlen
              && !memcmp (image + noid->off + noid->nhdr, oidbuf, oidlen)
              && noid->right)
            {
              if ( !idx-- )
                return noid->right;
            }
          
        }
    }
  return NULL;
}

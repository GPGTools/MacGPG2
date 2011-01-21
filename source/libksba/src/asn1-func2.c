/* asn1-func2.c - More ASN.1 definitions
 *      Copyright (C) 2000, 2001 Fabio Fiorina
 *      Copyright (C) 2001 Free Software Foundation, Inc.
 *      Copyright (C) 2008 g10 Code GmbH
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

/* 
   This file has functions which rely on on the asn1-gentables created
   asn1-tables.c - we can't put this into asn1-func.c because this one
   is needed by asn1-gentables ;-)
*/

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "util.h"
#include "ksba.h"
#include "asn1-func.h"


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




/**
 * Creates the structures needed to manage the ASN1 definitions. ROOT is
 * a vector created by the asn1-gentable tool.
 * 
 * Input Parameter: 
 *   
 *   Name of the module
 * 
 * Output Parameter:
 * 
 *   KsbaAsntree *result : return the pointer to an object to be used
 *   with other functions.
 * 
 * Return Value:
 *   0: structure created correctly. 
 *   GPG_ERR_GENERAL: an error occured while structure creation.  
 *   GPG_ERR_MODULE_NOT_FOUND: No such module NAME
 */
gpg_error_t
ksba_asn_create_tree (const char *mod_name, ksba_asn_tree_t *result)
{
  enum { DOWN, UP, RIGHT } move;
  const static_asn *root;
  const char *strgtbl;
  AsnNode pointer;
  AsnNode p = NULL;
  AsnNode p_last = NULL;
  unsigned long k;
  int rc;
  AsnNode link_next = NULL;

  if (!result)
    return gpg_error (GPG_ERR_INV_VALUE);
  *result = NULL;

  if (!mod_name)
    return gpg_error (GPG_ERR_INV_VALUE);
  root = _ksba_asn_lookup_table (mod_name, &strgtbl);
  if (!root)
    return gpg_error (GPG_ERR_MODULE_NOT_FOUND);

  pointer = NULL;
  move = UP;

  k = 0;
  while (root[k].stringvalue_off || root[k].type || root[k].name_off)
    {
      p = _ksba_asn_new_node (root[k].type);
      p->flags = root[k].flags;
      p->flags.help_down = 0;
      p->link_next = link_next;
      link_next = p;

      if (root[k].name_off)
	_ksba_asn_set_name (p, strgtbl + root[k].name_off);
      if (root[k].stringvalue_off)
        {
          if (root[k].type == TYPE_TAG)
            {
              unsigned long val;
              val = strtoul (strgtbl+root[k].stringvalue_off, NULL, 10);
              _ksba_asn_set_value (p, VALTYPE_ULONG, &val, sizeof(val));
            }
          else
            _ksba_asn_set_value (p, VALTYPE_CSTR, 
                                 strgtbl+root[k].stringvalue_off, 0);
        }

      if (!pointer)
	pointer = p;

      if (move == DOWN)
	set_down (p_last, p);
      else if (move == RIGHT)
	set_right (p_last, p);

      p_last = p;

      if (root[k].flags.help_down)
	move = DOWN;
      else if (root[k].flags.help_right)
	move = RIGHT;
      else
	{
	  while (1)
	    {
	      if (p_last == pointer)
		break;

	      p_last = find_up (p_last);

	      if (p_last == NULL)
		break;

	      if (p_last->flags.help_right)
		{
		  p_last->flags.help_right = 0;
		  move = RIGHT;
		  break;
		}
	    }
	}
      k++;
    }

  if (p_last == pointer)
    {
      ksba_asn_tree_t tree;

      _ksba_asn_change_integer_value (pointer);
      _ksba_asn_expand_object_id (pointer);
      tree = xtrymalloc (sizeof *tree + strlen (mod_name));
      if (!tree)
        rc = gpg_error (GPG_ERR_ENOMEM);
      else
        {
          tree->parse_tree = pointer;
          tree->node_list = p;
          strcpy (tree->filename, mod_name);
          *result = tree;
          rc = 0;
        }
    }
  else
      rc = gpg_error (GPG_ERR_GENERAL);

  if (rc)
    _ksba_asn_delete_structure (pointer);

  return rc;
}


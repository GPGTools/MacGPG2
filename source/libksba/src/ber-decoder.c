/* ber-decoder.c - Basic Encoding Rules Decoder
 *      Copyright (C) 2001, 2004, 2006 g10 Code GmbH
 *
 * This file is part of KSBA.
 *
 * KSBA is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * KSBA is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "util.h"

#include "util.h"
#include "ksba.h"
#include "asn1-func.h"
#include "ber-decoder.h"
#include "ber-help.h"


struct decoder_state_item_s {
  AsnNode node;
  int went_up;
  int in_seq_of;
  int in_any;    /* actually in a constructed any */
  int again;
  int next_tag;
  int length;  /* length of the value */
  int ndef_length; /* the length is of indefinite length */
  int nread;   /* number of value bytes processed */
};
typedef struct decoder_state_item_s DECODER_STATE_ITEM;

struct decoder_state_s {
  DECODER_STATE_ITEM cur;     /* current state */
  int stacksize;
  int idx;
  DECODER_STATE_ITEM stack[1];
};
typedef struct decoder_state_s *DECODER_STATE;


/* Context for a decoder. */
struct ber_decoder_s 
{
  AsnNode module;    /* the ASN.1 structure */
  ksba_reader_t reader;
  const char *last_errdesc; /* string with the error description */
  int non_der;    /* set if the encoding is not DER conform */
  AsnNode root;   /* of the expanded parse tree */
  DECODER_STATE ds;
  int bypass;

  /* Because some certificates actually come with trailing garbage, we
     use a hack to ignore this garbage.  This hack is enabled for data
     starting with a fixed length sequence and this variable takes the
     length of this sequence.  If it is 0, the hack is not
     activated. */
  unsigned long outer_sequence_length;
  int ignore_garbage;  /* Set to indicate that garbage should be
                          ignored. */
  int fast_stop;       /* Yet another hack.  */

  int first_tag_seen;  /* Indicates whether the first tag of a decoder
                          run has been read. */

  int honor_module_end; 
  int debug;
  int use_image;
  struct 
  {
    unsigned char *buf;
    size_t used;
    size_t length;
  } image;
  struct
  {
    int primitive;  /* current value is a primitive one */
    int length;     /* length of the primitive one */
    int nhdr;       /* length of the header */
    int tag; 
    int is_endtag;
    AsnNode node;   /* NULL or matching node */
  } val; 
};




static DECODER_STATE
new_decoder_state (void)
{
  DECODER_STATE ds;

  ds = xmalloc (sizeof (*ds) + 99*sizeof(DECODER_STATE_ITEM));
  ds->stacksize = 100;
  ds->idx = 0;
  ds->cur.node = NULL;
  ds->cur.went_up = 0;
  ds->cur.in_seq_of = 0;
  ds->cur.in_any = 0;
  ds->cur.again = 0;
  ds->cur.next_tag = 0;
  ds->cur.length = 0;
  ds->cur.ndef_length = 1;
  ds->cur.nread = 0;
  return ds;
}
       
static void        
release_decoder_state (DECODER_STATE ds)
{
  xfree (ds);
}

static void
dump_decoder_state (DECODER_STATE ds)
{
  int i;

  for (i=0; i < ds->idx; i++)
    {
      fprintf (stderr,"  ds stack[%d] (", i);
      if (ds->stack[i].node)
        _ksba_asn_node_dump (ds->stack[i].node, stderr);
      else
        fprintf (stderr, "Null");
      fprintf (stderr,") %s%d (%d)%s\n",
               ds->stack[i].ndef_length? "ndef ":"",
               ds->stack[i].length,
               ds->stack[i].nread,
               ds->stack[i].in_seq_of? " in_seq_of":"");
    }
}

/* Push ITEM onto the stack */
static void
push_decoder_state (DECODER_STATE ds)
{
  if (ds->idx >= ds->stacksize)
    {
      fprintf (stderr, "ERROR: decoder stack overflow!\n");
      abort ();
    }
  ds->stack[ds->idx++] = ds->cur;
}

static void
pop_decoder_state (DECODER_STATE ds)
{
  if (!ds->idx)
    {
      fprintf (stderr, "ERROR: decoder stack underflow!\n");
      abort ();
    }
  ds->cur = ds->stack[--ds->idx];
}



static int
set_error (BerDecoder d, AsnNode node, const char *text)
{
  fprintf (stderr,"ber-decoder: node `%s': %s\n", 
           node? node->name:"?", text);
  d->last_errdesc = text;
  return gpg_error (GPG_ERR_BAD_BER);
}


static int
eof_or_error (BerDecoder d, int premature)
{
  gpg_error_t err;

  err = ksba_reader_error (d->reader);
  if (err)
    {
      set_error (d, NULL, "read error");
      return err;
    }
  if (premature)
    return set_error (d, NULL, "premature EOF");
  return gpg_error (GPG_ERR_EOF);
}

static const char *
universal_tag_name (unsigned long no)
{
  static const char * const names[31] = {
    "[End Tag]",
    "BOOLEAN",
    "INTEGER",
    "BIT STRING",
    "OCTECT STRING",
    "NULL",
    "OBJECT IDENTIFIER",
    "ObjectDescriptor",
    "EXTERNAL",
    "REAL",
    "ENUMERATED",
    "EMBEDDED PDV",
    "UTF8String",
    "RELATIVE-OID",
    "[UNIVERSAL 14]",
    "[UNIVERSAL 15]",
    "SEQUENCE",
    "SET",
    "NumericString",
    "PrintableString",
    "TeletexString",
    "VideotexString",
    "IA5String",
    "UTCTime",
    "GeneralizedTime",
    "GraphicString",
    "VisibleString",
    "GeneralString",
    "UniversalString",
    "CHARACTER STRING",
    "BMPString"
  };

  return no < DIM(names)? names[no]:NULL;
}


static void
dump_tlv (const struct tag_info *ti, FILE *fp)
{
  const char *tagname = NULL;

  if (ti->class == CLASS_UNIVERSAL)
    tagname = universal_tag_name (ti->tag);

  if (tagname)
    fputs (tagname, fp);
  else
    fprintf (fp, "[%s %lu]", 
             ti->class == CLASS_UNIVERSAL? "UNIVERSAL" :
             ti->class == CLASS_APPLICATION? "APPLICATION" :
             ti->class == CLASS_CONTEXT? "CONTEXT-SPECIFIC" : "PRIVATE",
             ti->tag);
  fprintf (fp, " %c hdr=%lu len=",
           ti->is_constructed? 'c':'p',
           (unsigned long)ti->nhdr);
  if (ti->ndef)
    fputs ("ndef", fp);
  else
    fprintf (fp, "%lu", ti->length);
}


static void
clear_help_flags (AsnNode node)
{
  AsnNode p;

  for (p=node; p; p = _ksba_asn_walk_tree (node, p))
    {
      if (p->type == TYPE_TAG)
        {
          p->flags.tag_seen = 0;
        }
      p->flags.skip_this = 0;
    }
  
}

static void
prepare_copied_tree (AsnNode node)
{
  AsnNode p;

  clear_help_flags (node);
  for (p=node; p; p = _ksba_asn_walk_tree (node, p))
    p->off = -1;
  
}

static void
fixup_type_any (AsnNode node)
{
  AsnNode p;

  for (p=node; p; p = _ksba_asn_walk_tree (node, p))
    {
      if (p->type == TYPE_ANY && p->off != -1)
        p->type = p->actual_type;
    }
}



BerDecoder
_ksba_ber_decoder_new (void)
{
  BerDecoder d;

  d = xtrycalloc (1, sizeof *d);
  if (!d)
    return NULL;

  return d;
}

void
_ksba_ber_decoder_release (BerDecoder d)
{
  xfree (d);
}

/**
 * _ksba_ber_decoder_set_module:
 * @d: Decoder object 
 * @module: ASN.1 Parse tree
 * 
 * Initialize the decoder with the ASN.1 module.  Note, that this is a
 * shallow copy of the module.  Hmmm: What about ref-counting of
 * AsnNodes?
 * 
 * Return value: 0 on success or an error code
 **/
gpg_error_t
_ksba_ber_decoder_set_module (BerDecoder d, ksba_asn_tree_t module)
{
  if (!d || !module)
    return gpg_error (GPG_ERR_INV_VALUE);
  if (d->module)
    return gpg_error (GPG_ERR_CONFLICT); /* module already set */

  d->module = module->parse_tree;
  return 0;
}


gpg_error_t
_ksba_ber_decoder_set_reader (BerDecoder d, ksba_reader_t r)
{
  if (!d || !r)
    return gpg_error (GPG_ERR_INV_VALUE);
  if (d->reader)
    return gpg_error (GPG_ERR_CONFLICT); /* reader already set */
  
  d->reader = r;
  return 0;
}


/**********************************************
 ***********  decoding machinery  *************
 **********************************************/

/* Read one byte, return that byte or -1 on error or EOF. */
static int
read_byte (ksba_reader_t reader)
{
  unsigned char buf;
  size_t nread;
  int rc;

  do
    rc = ksba_reader_read (reader, &buf, 1, &nread);
  while (!rc && !nread);
  return rc? -1: buf;
}

/* Read COUNT bytes into buffer.  BUFFER may be NULL to skip over
   COUNT bytes.  Return 0 on success or -1 on error. */
static int 
read_buffer (ksba_reader_t reader, char *buffer, size_t count)
{
  size_t nread;

  if (buffer)
    {
      while (count)
        {
          if (ksba_reader_read (reader, buffer, count, &nread))
            return -1;
          buffer += nread;
          count -= nread;
        }
    }
  else
    {
      char dummy[256];
      size_t n;

      while (count)
        {
          n = count > DIM(dummy) ? DIM(dummy): count;
          if (ksba_reader_read (reader, dummy, n, &nread))
            return -1;
          count -= nread;
        }
    }
  return 0;
}

/* Return 0 for no match, 1 for a match and 2 for an ANY match of an
   constructed type */
static int
cmp_tag (AsnNode node, const struct tag_info *ti)
{
  if (node->flags.class != ti->class)
    {
      if (node->flags.class == CLASS_UNIVERSAL && node->type == TYPE_ANY)
        return ti->is_constructed? 2:1; 
      return 0;
    }
  if (node->type == TYPE_TAG)
    {
      return_val_if_fail (node->valuetype == VALTYPE_ULONG, 0);
      return node->value.v_ulong == ti->tag;
    }
  if (node->type == ti->tag)
    return 1;
  if (ti->class == CLASS_UNIVERSAL)
    {
      if (node->type == TYPE_SEQUENCE_OF && ti->tag == TYPE_SEQUENCE)
        return 1;
      if (node->type == TYPE_SET_OF && ti->tag == TYPE_SET)
        return 1;
      if (node->type == TYPE_ANY)
        return _ksba_asn_is_primitive (ti->tag)? 1:2; 
    }

  return 0;
}

/* Find the node in the tree ROOT corresponding to TI and return that
   node.  Returns NULL if the node was not found */
static AsnNode
find_anchor_node (AsnNode root, const struct tag_info *ti)
{
  AsnNode node = root;

  while (node)
    {
      if (cmp_tag (node, ti))
        {
          return node; /* found */
        }

      if (node->down)
        node = node->down;
      else if (node == root)
        return NULL; /* not found */
      else if (node->right)
        node = node->right;
      else 
        { /* go up and right */
          do
            {
              while (node->left && node->left->right == node)
                node = node->left;
              node = node->left;
              
              if (!node || node == root)
                return NULL; /* back at the root -> not found */
            }
          while (!node->right);
          node = node->right;
        }
    }

  return NULL;
}

static int
match_der (AsnNode root, const struct tag_info *ti,
           DECODER_STATE ds, AsnNode *retnode, int debug)
{
  int rc;
  AsnNode node;

  *retnode = NULL;
  node = ds->cur.node;
  if (!node)
    {
      if (debug)
        fputs ("  looking for anchor\n", stderr);
      node = find_anchor_node (root,  ti);
      if (!node)
        fputs (" anchor node not found\n", stderr);
    }
  else if (ds->cur.again)
    {
      if (debug)
        fputs ("  doing last again\n", stderr);
      ds->cur.again = 0;
    }
  else if (_ksba_asn_is_primitive (node->type) || node->type == TYPE_ANY
           || node->type == TYPE_SIZE || node->type == TYPE_DEFAULT )
    {
      if (debug)
        fputs ("  primitive type - get next\n", stderr);
      if (node->right)
        node = node->right;
      else if (!node->flags.in_choice)
        {
          if (node->flags.is_implicit)
            {
              if (debug)
                fputs ("  node was implicit - advancing\n", stderr);
              while (node->left && node->left->right == node)
                node = node->left;
              node = node->left; /* this is the up pointer */
              if (node)
                node = node->right;
            }
          else
            node = NULL;
        }
      else /* in choice */
        {
          if (debug)
            fputs ("  going up after choice - get next\n", stderr);
          while (node->left && node->left->right == node)
            node = node->left;
          node = node->left; /* this is the up pointer */
          if (node)
            node = node->right;
        }
    }
  else if (node->type == TYPE_SEQUENCE_OF || node->type == TYPE_SET_OF)
    {
      if (debug)
        {
          fprintf (stderr, "  prepare for seq/set_of (%d %d)  ",
                  ds->cur.length, ds->cur.nread);
          fprintf (stderr, "  cur: ("); _ksba_asn_node_dump (node, stderr);
          fprintf (stderr, ")\n");
          if (ds->cur.node->flags.in_array)
            fputs ("  This is in an array!\n", stderr);
          if (ds->cur.went_up)
            fputs ("  And we are going up!\n", stderr);
        }
      if ((ds->cur.went_up && !ds->cur.node->flags.in_array) ||
          (ds->idx && ds->cur.nread >= ds->stack[ds->idx-1].length))
        {
          if (debug)
            fprintf (stderr, "  advancing\n");
          if (node->right)
            node = node->right;
          else
            {
              for (;;)
                {
                  while (node->left && node->left->right == node)
                    node = node->left;
                  node = node->left; /* this is the up pointer */
                  if (!node)
                    break;
                  if (node->right)
                    {
                      node = node->right;
                      break;
                    }
                }
            }
        }
      else if (ds->cur.node->flags.in_array
               && ds->cur.went_up)
        {
          if (debug)
            fputs ("  Reiterating\n", stderr);
          node = _ksba_asn_insert_copy (node);
          if (node)
            prepare_copied_tree (node);
        }
      else
        node = node->down;
    }
  else /* constructed */
    {
      if (debug)
        {
          fprintf (stderr, "  prepare for constructed (%d %d) ",
                  ds->cur.length, ds->cur.nread);
          fprintf (stderr, "  cur: ("); _ksba_asn_node_dump (node, stderr);
          fprintf (stderr, ")\n");
          if (ds->cur.node->flags.in_array)
            fputs ("  This is in an array!\n", stderr);
          if (ds->cur.went_up)
            fputs ("  And we are going up!\n", stderr);
        }
      ds->cur.in_seq_of = 0;

      if (ds->cur.node->flags.in_array
          && ds->cur.went_up)
        {
          if (debug)
            fputs ("  Reiterating this\n", stderr);
          node = _ksba_asn_insert_copy (node);
          if (node)
            prepare_copied_tree (node);
        }
      else if (ds->cur.went_up || ds->cur.next_tag || ds->cur.node->flags.skip_this)
        {
          if (node->right)
            node = node->right;
          else
            {
              for (;;)
                {
                  while (node->left && node->left->right == node)
                    node = node->left;
                  node = node->left; /* this is the up pointer */
                  if (!node)
                    break;
                  if (node->right)
                    {
                      node = node->right;
                      break;
                    }
                }
            }
        }
      else 
        node = node->down;
      
    }
  if (!node)
    return -1;
  ds->cur.node = node;
  ds->cur.went_up = 0;
  ds->cur.next_tag = 0;

  if (debug)
    {
      fprintf (stderr, "  Expect ("); _ksba_asn_node_dump (node,stderr); fprintf (stderr, ")\n");
    }

  if (node->flags.skip_this)
    {
      if (debug)
        fprintf (stderr, "   skipping this\n");
      return 1;
    }

  if (node->type == TYPE_SIZE)
    {
      if (debug)
        fprintf (stderr, "   skipping size tag\n");
      return 1;
    }
  if (node->type == TYPE_DEFAULT)
    {
      if (debug)
        fprintf (stderr, "   skipping default tag\n");
      return 1;
    }

  if (node->flags.is_implicit)
    {
      if (debug)
        fprintf (stderr, "   dummy accept for implicit tag\n");
      return 1; /* again */
    }

  if ( (rc=cmp_tag (node, ti)))
    {
      *retnode = node;
      return rc==2? 4:3;
    }
    
  if (node->type == TYPE_CHOICE)
    {
      if (debug)
        fprintf (stderr, "   testing choice...\n");
      for (node = node->down; node; node = node->right)
        {
          if (debug)
            {
              fprintf (stderr, "       %s (", node->flags.skip_this? "skip":" cmp");
              _ksba_asn_node_dump (node, stderr);
              fprintf (stderr, ")\n");
            }

          if (!node->flags.skip_this && cmp_tag (node, ti) == 1)
            {
              if (debug)
                {
                  fprintf (stderr, "  choice match <"); dump_tlv (ti, stderr);
                  fprintf (stderr, ">\n");
                }
              /* mark the remaining as done */
              for (node=node->right; node; node = node->right)
                  node->flags.skip_this = 1;
              return 1;
            }
          node->flags.skip_this = 1;

        }
      node = ds->cur.node; /* reset */
    }

  if (node->flags.in_choice)
    {
      if (debug)
        fprintf (stderr, "   skipping non matching choice\n");
      return 1;
    }
  
  if (node->flags.is_optional)
    {
      if (debug)
        fprintf (stderr, "   skipping optional element\n");
      if (node->type == TYPE_TAG)
        ds->cur.next_tag = 1;
      return 1;
    }

  if (node->flags.has_default)
    {
      if (debug)
        fprintf (stderr, "   use default value\n");
      if (node->type == TYPE_TAG)
        ds->cur.next_tag = 1;
      *retnode = node;
      return 2;
    }

  return -1;
}


static gpg_error_t 
decoder_init (BerDecoder d, const char *start_name)
{
  d->ds = new_decoder_state ();

  d->root = _ksba_asn_expand_tree (d->module, start_name);
  clear_help_flags (d->root);
  d->bypass = 0;
  if (d->debug)
    fprintf (stderr, "DECODER_INIT for `%s'\n", start_name? start_name: "[root]");
  return 0;
}

static void
decoder_deinit (BerDecoder d)
{
  release_decoder_state (d->ds);
  d->ds = NULL;
  d->val.node = NULL;
  if (d->debug)
    fprintf (stderr, "DECODER_DEINIT\n");
}


static gpg_error_t
decoder_next (BerDecoder d)
{
  struct tag_info ti;
  AsnNode node = NULL;
  gpg_error_t err;
  DECODER_STATE ds = d->ds;
  int debug = d->debug;

  if (d->ignore_garbage && d->fast_stop)
    {
      /* I am not anymore sure why we have this ignore_garbage
         machinery: The whole decoder needs and overhaul; it seems not
         to honor the length specification and runs for longer than
         expected. 

         This here is another hack to not eat up an end tag - this is
         required in in some cases and in theory should be used always
         but we want to avoid any regression, thus this flag.  */
      return gpg_error (GPG_ERR_EOF);
    }

  err = _ksba_ber_read_tl (d->reader, &ti);
  if (err)
    {
      if (debug)
        fprintf (stderr, "ber_read_tl error: %s (%s)\n",
                 gpg_strerror (err), ti.err_string? ti.err_string:"");
      /* This is our actual hack to cope with some trailing garbage:
         Only if we get an premature EOF and we know that we have read
         the complete certificate we change the error to EOF.  This
         won't help with all kinds of garbage but it fixes the case
         where just one byte is appended.  This is for example the
         case with current Siemens certificates.  This approach seems
         to be the least intrusive one. */
      if (gpg_err_code (err) == GPG_ERR_BAD_BER
          && d->ignore_garbage
          && ti.err_string && !strcmp (ti.err_string, "premature EOF"))
        err = gpg_error (GPG_ERR_EOF);
      return err;
    }

  if (debug)
    {
      fprintf (stderr, "ReadTLV <");
      dump_tlv (&ti, stderr);
      fprintf (stderr, ">\n");
    }

  /* Check whether the trailing garbage hack is required. */
  if (!d->first_tag_seen)
    {
      d->first_tag_seen = 1;
      if (ti.tag == TYPE_SEQUENCE && ti.length && !ti.ndef)
        d->outer_sequence_length = ti.length;
    }

  /* Store stuff in the image buffer. */
  if (d->use_image)
    {
      if (!d->image.buf)
        {
          /* We need some extra bytes to store the stuff we read ahead
             at the end of the module which is later pushed back. */
          d->image.length = ti.length + 100;
          d->image.used = 0;
          d->image.buf = xtrymalloc (d->image.length);
          if (!d->image.buf)
            return gpg_error (GPG_ERR_ENOMEM);
        }

      if (ti.nhdr + d->image.used >= d->image.length)
        return set_error (d, NULL, "image buffer too short to store the tag");

      memcpy (d->image.buf + d->image.used, ti.buf, ti.nhdr);
      d->image.used += ti.nhdr;
    }
  

  if (!d->bypass)
    {
      int again, endtag;

      do
        {
          again = endtag = 0;
          switch ( ds->cur.in_any? 4 
                   : (ti.class == CLASS_UNIVERSAL && !ti.tag)? (endtag=1,5)
                   : match_der (d->root, &ti, ds, &node, debug))
            { 
            case -1:
              if (debug)
                {
                  fprintf (stderr, "   FAIL <");
                  dump_tlv (&ti, stderr); 
                  fprintf (stderr, ">\n");
                }
              if (d->honor_module_end)
                {
                  /* We must push back the stuff we already read */
                  ksba_reader_unread (d->reader, ti.buf, ti.nhdr);
                  return gpg_error (GPG_ERR_EOF); 
                }
              else
                d->bypass = 1;
              break;
            case 0:
              if (debug)
                fputs ("  End of description\n", stderr);
              d->bypass = 1;
              break;
            case 1: /* again */
              if (debug)
                fprintf (stderr, "  Again\n");
              again = 1;
              break;
            case 2: /* Use default value +  again */
              if (debug)
                fprintf (stderr, "  Using default\n");
              again = 1;
              break;
            case 4: /* Match of ANY on a constructed type */
              if (debug)
                  fprintf (stderr, "  ANY");
              ds->cur.in_any = 1;
            case 3: /* match */ 
            case 5: /* end tag */
              if (debug)
                {
                  fprintf (stderr, "  Match <");
                  dump_tlv (&ti, stderr);
                  fprintf (stderr, ">\n");
                }
              /* Increment by the header length */
              ds->cur.nread += ti.nhdr;
                  
              if (!ti.is_constructed)
                  ds->cur.nread += ti.length;

              ds->cur.went_up = 0;
              do
                {
                  if (debug)
                    fprintf (stderr, "  (length %d nread %d) %s\n",
                            ds->idx? ds->stack[ds->idx-1].length:-1,
                            ds->cur.nread,
                            ti.is_constructed? "con":"pri");
                  if (d->outer_sequence_length
                      && ds->idx == 1
                      && ds->cur.nread == d->outer_sequence_length)
                    {
                      if (debug)
                        fprintf (stderr, "  Need to stop now\n");
                      d->ignore_garbage = 1;
                    }

                  if ( ds->idx
                       && !ds->stack[ds->idx-1].ndef_length
                       && (ds->cur.nread
                           > ds->stack[ds->idx-1].length)) 
                    {
                      fprintf (stderr, "  ERROR: object length field "
                               "%d octects too large\n",   
                              ds->cur.nread > ds->cur.length);
                      ds->cur.nread = ds->cur.length;
                    }
                  if ( ds->idx
                       && (endtag
                           || (!ds->stack[ds->idx-1].ndef_length
                               && (ds->cur.nread
                                   >= ds->stack[ds->idx-1].length)))) 
                    {
                      int n = ds->cur.nread;
                      pop_decoder_state (ds);
                      ds->cur.nread += n;
                      ds->cur.went_up++;
                    }
                  endtag = 0;
                }
              while ( ds->idx
                      && !ds->stack[ds->idx-1].ndef_length
                      && (ds->cur.nread
                          >= ds->stack[ds->idx-1].length));
                  
              if (ti.is_constructed && (ti.length || ti.ndef))
                {
                  /* prepare for the next level */
                  ds->cur.length = ti.length;
                  ds->cur.ndef_length = ti.ndef;
                  push_decoder_state (ds);
                  ds->cur.length = 0;
                  ds->cur.ndef_length = 0;
                  ds->cur.nread = 0;
                }
              if (debug)
                fprintf (stderr, "  (length %d nread %d) end\n",
                        ds->idx? ds->stack[ds->idx-1].length:-1,
                        ds->cur.nread);
              break;
            default:
              never_reached ();
              abort (); 
              break;
            }
        }
      while (again);
    }

  d->val.primitive = !ti.is_constructed;
  d->val.length = ti.length;
  d->val.nhdr = ti.nhdr;
  d->val.tag  = ti.tag; /* kludge to fix TYPE_ANY probs */
  d->val.is_endtag = (ti.class == CLASS_UNIVERSAL && !ti.tag); 
  d->val.node = d->bypass? NULL : node;
  if (debug)
    dump_decoder_state (ds);
  
  return 0;
}

static gpg_error_t
decoder_skip (BerDecoder d)
{
  if (d->val.primitive)
    { 
      if (read_buffer (d->reader, NULL, d->val.length))
        return eof_or_error (d, 1);
    }
  return 0;
}



/* Calculate the distance between the 2 nodes */
static int
distance (AsnNode root, AsnNode node)
{
  int n=0;

  while (node && node != root)
    {
      while (node->left && node->left->right == node)
        node = node->left;
      node = node->left;
      n++;
    }

  return n;
}


/**
 * _ksba_ber_decoder_dump:
 * @d: Decoder object
 * 
 * Dump a textual representation of the encoding to the given stream.
 * 
 * Return value: 
 **/
gpg_error_t
_ksba_ber_decoder_dump (BerDecoder d, FILE *fp)
{
  gpg_error_t err;
  int depth = 0;
  AsnNode node;
  unsigned char *buf = NULL;
  size_t buflen = 0;;

  if (!d)
    return gpg_error (GPG_ERR_INV_VALUE);

#ifdef HAVE_GETENV
  d->debug = !!getenv("KSBA_DEBUG_BER_DECODER");
#else
  d->debug = 0;
#endif
  d->use_image = 0;
  d->image.buf = NULL;
  err = decoder_init (d, NULL);
  if (err)
    return err;

  while (!(err = decoder_next (d)))
    {
      node = d->val.node;
      if (node)
        depth = distance (d->root, node);

      fprintf (fp, "%4lu %4u:%*s",
               ksba_reader_tell (d->reader) - d->val.nhdr,
               d->val.length,
               depth*2, "");
      if (node)
        _ksba_asn_node_dump (node, fp);
      else
        fputs ("[No matching node]", fp);

      if (node && d->val.primitive)
        {
          int i, n, c;
          char *p;
      
          if (!buf || buflen < d->val.length)
            {
              xfree (buf);
              buflen = d->val.length + 100;
              buf = xtrymalloc (buflen);
              if (!buf)
                err = gpg_error (GPG_ERR_ENOMEM);
            }

          for (n=0; !err && n < d->val.length; n++)
            {
              if ( (c=read_byte (d->reader)) == -1)
                err = eof_or_error (d, 1);
              buf[n] = c;
            }
          if (err)
            break;
          fputs ("  (", fp);
          p = NULL;
          switch (node->type)
            {
            case TYPE_OBJECT_ID:
              p = ksba_oid_to_str (buf, n);
              break;
            default:
              for (i=0; i < n && i < 20; i++)
                fprintf (fp,"%02x", buf[i]);
              if (i < n)
                fputs ("..more..", fp);
              break;
            }
          if (p)
            {
              fputs (p, fp);
              xfree (p);
            }
          fputs (")\n", fp);
        }
      else
        {
          err = decoder_skip (d);
          putc ('\n', fp);
        }
      if (err)
        break;

    }
  if (gpg_err_code (err) == GPG_ERR_EOF)
    err = 0;

  decoder_deinit (d);
  xfree (buf);
  return err;
}




gpg_error_t
_ksba_ber_decoder_decode (BerDecoder d, const char *start_name,
                          unsigned int flags,
                          AsnNode *r_root,
                          unsigned char **r_image, size_t *r_imagelen)
{
  gpg_error_t err;
  AsnNode node;
  unsigned char *buf = NULL;
  size_t buflen = 0;
  unsigned long startoff;

  if (!d)
    return gpg_error (GPG_ERR_INV_VALUE);

  if (r_root)
    *r_root = NULL;

#ifdef HAVE_GETENV
  d->debug = !!getenv("KSBA_DEBUG_BER_DECODER");
#else
  d->debug = 0;
#endif
  d->honor_module_end = 1;
  d->use_image = 1;
  d->image.buf = NULL;
  d->fast_stop = !!(flags & BER_DECODER_FLAG_FAST_STOP);

  startoff = ksba_reader_tell (d->reader);

  err = decoder_init (d, start_name);
  if (err)
    return err;

  while (!(err = decoder_next (d)))
    {
      int n, c;

      node = d->val.node;
      /* Fixme: USE_IMAGE is only not used with the ber-dump utility
         and thus of no big use.  We should remove the other code
         paths and dump ber-dump.c. */
      if (d->use_image)
        {
          if (node && !d->val.is_endtag)
            { /* We don't have nodes for the end tag - so don't store it */
              node->off = (ksba_reader_tell (d->reader)
                           - d->val.nhdr - startoff);
              node->nhdr = d->val.nhdr;
              node->len = d->val.length;
              if (node->type == TYPE_ANY)
                node->actual_type = d->val.tag;
            }
          if (d->image.used + d->val.length > d->image.length)
            err = set_error(d, NULL, "TLV length too large");
          else if (d->val.primitive)
            {
              if( read_buffer (d->reader,
                               d->image.buf + d->image.used, d->val.length))
                err = eof_or_error (d, 1);
              else
                d->image.used += d->val.length;
            }
        }
      else if (node && d->val.primitive)
        {
          if (!buf || buflen < d->val.length)
            {
              xfree (buf);
              buflen = d->val.length + 100;
              buf = xtrymalloc (buflen);
              if (!buf)
                err = gpg_error (GPG_ERR_ENOMEM);
            }

          for (n=0; !err && n < d->val.length; n++)
            {
              if ( (c=read_byte (d->reader)) == -1)
                err = eof_or_error (d, 1);
              buf[n] = c;
            }
          if (err)
            break;

          switch (node->type)
            {
            default:
              _ksba_asn_set_value (node, VALTYPE_MEM, buf, n);
              break;
            }
        }
      else
        {
          err = decoder_skip (d);
        }
      if (err)
        break;
    }
  if (gpg_err_code (err) == GPG_ERR_EOF)
    err = 0;

  if (err)
    xfree (d->image.buf);

  if (r_root && !err)
    {
      if (!d->image.buf)
        { /* Not even the first node available - return eof */
	  _ksba_asn_release_nodes (d->root);
          d->root = NULL;
          err = gpg_error (GPG_ERR_EOF);
        }
      
      fixup_type_any (d->root);
      *r_root = d->root;
      d->root = NULL;
      *r_image = d->image.buf;
      d->image.buf = NULL;
      *r_imagelen = d->image.used;
      if (d->debug)
        {
          fputs ("Value Tree:\n", stderr); 
          _ksba_asn_node_dump_all (*r_root, stderr); 
        }
    }

  decoder_deinit (d);
  xfree (buf);
  return err;
}



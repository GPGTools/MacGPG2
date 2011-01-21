/* asn1-gentables.c - Tool to create required ASN tables
 *      Copyright (C) 2001, 2008 g10 Code GmbH
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>

#include "gen-help.h"
#include "asn1-func.h"

#define PGMNAME "asn1-gentables"

#if (__GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 5 ))
# define  ATTR_PRINTF(a,b)  __attribute__ ((format (printf,a,b)))
#else
# define  ATTR_PRINTF(a,b) 
#endif

#ifdef _WIN32
#define DEVNULL_NAME "nul"
#else
#define DEVNULL_NAME "/dev/null"
#endif


/* keep track of parsing error */
static int error_counter;

/* option --dump */
static int dump_only;
/* option --check */
static int check_only;

struct name_list_s {
  struct name_list_s *next;
  char name[1];
};

static struct name_list_s *string_table, **string_table_tail;
static size_t string_table_offset;

static void print_error (const char *fmt, ... )  ATTR_PRINTF(1,2);


static void
print_error (const char *fmt, ... )  
{
  va_list arg_ptr ;

  va_start (arg_ptr, fmt);
  fputs (PGMNAME ": ", stderr);
  vfprintf (stderr, fmt, arg_ptr);
  va_end (arg_ptr);
  error_counter++;
  
}

static size_t
insert_string (const char *name)
{
  struct name_list_s *item;
  size_t off, n;

  if (!string_table_tail)
    {
      string_table_tail = &string_table;
      insert_string ("");
    }
  
  if (string_table_offset && !*name)
    return 0;

  for (item = string_table,off = 0; item; item = item->next)
    {
      for (n=0; item->name[n]; n++)
        if (!strcmp (item->name+n, name))
          return off + n;
      off += strlen (item->name) + 1;
    }
  
  item = xmalloc ( sizeof *item + strlen (name));
  strcpy (item->name, name);
  item->next = NULL;
  *string_table_tail = item;
  string_table_tail = &item->next;
  off = string_table_offset;
  string_table_offset += strlen (name) + 1;
  return off;
}

static int
cmp_string (const void *aptr, const void *bptr)
{
  const struct name_list_s **a = (const struct name_list_s **)aptr;
  const struct name_list_s **b = (const struct name_list_s **)bptr;

  return strlen ((*a)->name) < strlen ((*b)->name);
}

static void
sort_string_table (void)
{
  struct name_list_s *item;
  struct name_list_s **array;
  size_t i, arraylen;
  
  if (!string_table || !string_table->next)
    return; /* Nothing to sort.  */

  for (item = string_table,arraylen = 0; item; item = item->next)
    arraylen++;
  array = xcalloc (arraylen, sizeof *array);
  for (item = string_table,arraylen = 0; item; item = item->next)
    array[arraylen++] = item;
  qsort (array, arraylen, sizeof *array, cmp_string);
  /* Replace table by sorted one.  */
  string_table_tail = NULL;
  string_table = NULL;
  string_table_offset = 0;
  for (i=0; i < arraylen; i++)
    insert_string (array[i]->name);
  xfree (array);
  /* for (item = string_table,arraylen = 0; item; item = item->next) */
  /*   fprintf (stderr, "  `%s'\n", item->name); */
}


static void
write_string_table (FILE *fp)
{
  struct name_list_s *item;
  const char *s;
  int count = 0;
  int pos;

  if (!string_table)
    insert_string ("");

  fputs ("static const char string_table[] = {\n  ", fp);
  for (item = string_table; item; item = item->next)
    {
      for (s=item->name, pos=0; *s; s++)
        {
          if (!(pos++ % 16)) 
            fprintf (fp, "%s  ", pos>1? "\n":"");
          fprintf (fp, "'%c',", *s);
        }
      fputs ("'\\0',\n", fp);
      count++;
    }
  /* (we use an extra \0 to get rid of the last comma) */
  fprintf (fp, "  '\\0' };\n/* (%d strings) */\n", count);
}


static struct name_list_s *
create_static_structure (AsnNode pointer, const char *file_name, FILE *fp)
{
  AsnNode p;
  struct name_list_s *structure_name;
  const char *char_p, *slash_p, *dot_p;
  char numbuf[50];

  char_p = file_name;
  slash_p = file_name;
  while ((char_p = strchr (char_p, '/')))
    {
      char_p++;
      slash_p = char_p;
    }

  char_p = slash_p;
  dot_p = file_name + strlen (file_name);

  while ((char_p = strchr (char_p, '.')))
    {
      dot_p = char_p;
      char_p++;
    }

  structure_name = xmalloc (sizeof *structure_name + dot_p - slash_p + 100);
  structure_name->next = NULL;
  memcpy (structure_name->name, slash_p, dot_p - slash_p);
  structure_name->name[dot_p - slash_p] = 0;

  fprintf (fp, "static const static_asn %s_asn1_tab[] = {\n",
           structure_name->name);

  for (p = pointer; p; p = _ksba_asn_walk_tree (pointer, p))
    {
      /* set the help flags */
      p->flags.help_down  = !!p->down;
      p->flags.help_right = !!p->right;

      /* write a structure line */
      fputs ("  {", fp);
      if (p->name)
	fprintf (fp, "%u", (unsigned int)insert_string (p->name));
      else
	fprintf (fp, "0");
      fprintf (fp, ",%u", p->type);

      fputs (", {", fp);
      fprintf (fp, "%u", p->flags.class);
      fputs (p->flags.explicit       ? ",1":",0", fp);
      fputs (p->flags.implicit       ? ",1":",0", fp);
      fputs (p->flags.has_imports    ? ",1":",0", fp);
      fputs (p->flags.assignment     ? ",1":",0", fp);
      fputs (p->flags.one_param      ? ",1":",0", fp);
      fputs (p->flags.has_tag        ? ",1":",0", fp);
      fputs (p->flags.has_size       ? ",1":",0", fp);
      fputs (p->flags.has_list       ? ",1":",0", fp);
      fputs (p->flags.has_min_max    ? ",1":",0", fp);
      fputs (p->flags.has_defined_by ? ",1":",0", fp);
      fputs (p->flags.is_false       ? ",1":",0", fp);
      fputs (p->flags.is_true        ? ",1":",0", fp);
      fputs (p->flags.has_default     ? ",1":",0", fp);
      fputs (p->flags.is_optional    ? ",1":",0", fp);
      fputs (p->flags.is_implicit    ? ",1":",0", fp);
      fputs (p->flags.in_set         ? ",1":",0", fp);
      fputs (p->flags.in_choice      ? ",1":",0", fp);
      fputs (p->flags.in_array       ? ",1":",0", fp);
      fputs (p->flags.is_any         ? ",1":",0", fp);
      fputs (p->flags.not_used       ? ",1":",0", fp);
      fputs (p->flags.help_down      ? ",1":",0", fp);
      fputs (p->flags.help_right     ? ",1":",0", fp);
      fputs ("}", fp);

      if (p->valuetype == VALTYPE_CSTR)
	fprintf (fp, ",%u", 
                 (unsigned int)insert_string (p->value.v_cstr));
      else if (p->valuetype == VALTYPE_LONG
               && p->type == TYPE_INTEGER && p->flags.assignment)
        {
          snprintf (numbuf, sizeof numbuf, "%ld", p->value.v_long);
          fprintf (fp, ",%u", (unsigned int)insert_string (numbuf));
        }
      else if (p->valuetype == VALTYPE_ULONG)
        {
          snprintf (numbuf, sizeof numbuf, "%lu", p->value.v_ulong);
          fprintf (fp, ",%u", (unsigned int)insert_string (numbuf));
        }
      else
        {
          if (p->valuetype)
            print_error ("can't store a value of type %d\n", p->valuetype);
          fprintf (fp, ",0");
        }
      fputs ("},\n", fp);
    }

  fprintf (fp, "  {0,0}\n};\n");

  return structure_name;
}



static struct name_list_s *
one_file (const char *fname, int *count, FILE *fp)
{
  ksba_asn_tree_t tree;
  int rc;
    
  rc = ksba_asn_parse_file (fname, &tree, check_only);
  if (rc)
    print_error ("error parsing `%s': %s\n", fname, gpg_strerror (rc) );
  else if (!check_only)
    {
      if (dump_only)
        ksba_asn_tree_dump (tree, dump_only==2? "<":NULL, fp);
      else
        {
          if (!*count)
            fprintf (fp,"\n"
                     "#include <config.h>\n"
                     "#include <stdio.h>\n"
                     "#include <string.h>\n"
                     "#include \"ksba.h\"\n"
                     "#include \"asn1-func.h\"\n"
                     "\n");
          ++*count;
          return create_static_structure (tree->parse_tree, fname, fp);
        }
    }
  return 0;
}


int
main (int argc, char **argv)
{
  int count = 0;
  struct name_list_s *all_names = NULL, *nl;
  int i;

  if (!argc || (argc > 1 &&
                (!strcmp (argv[1],"--help") || !strcmp (argv[1],"-h"))) )
    {
      fputs ("usage: asn1-gentables [--check] [--dump[-expanded]] [files.asn]\n",
             stderr);
      return 0;
    }
  
  argc--; argv++;
  if (argc && !strcmp (*argv,"--check"))
    {
      argc--; argv++;
      check_only = 1;
    }
  else if (argc && !strcmp (*argv,"--dump"))
    {
      argc--; argv++;
      dump_only = 1;
    }
  else if (argc && !strcmp (*argv,"--dump-expanded"))
    {
      argc--; argv++;
      dump_only = 2;
    }


  if (!argc)
    all_names = one_file ("-", &count, stdout);
  else
    {
      FILE *nullfp;

      /* We first parse it to /dev/null to build up the string table.  */
      nullfp = fopen (DEVNULL_NAME, "w");
      if (!nullfp)
        {
          print_error ("can't open `%s': %s\n", DEVNULL_NAME, strerror (errno));
          exit (2);
        }
      for (i=0; i < argc; i++) 
        one_file (argv[i], &count, nullfp);
      fclose (nullfp);

      sort_string_table ();

      count = 0;
      for (; argc; argc--, argv++) 
        {
          nl = one_file (*argv, &count, stdout);
          if (nl)
            {
              nl->next = all_names;
              all_names = nl;
            }
        }
    }

  if (all_names && !error_counter)
    { 
      /* Write the string table. */
      putchar ('\n');
      write_string_table (stdout);
      /* Write the lookup function */
      printf ("\n\nconst static_asn *\n"
              "_ksba_asn_lookup_table (const char *name,"
              " const char **stringtbl)\n"
              "{\n"
              "  *stringtbl = string_table;\n"
              );
      for (nl=all_names; nl; nl = nl->next)
        printf ("  if (!strcmp (name, \"%s\"))\n"
                "    return %s_asn1_tab;\n", nl->name, nl->name);
      printf ("\n  return NULL;\n}\n");
    }

  return error_counter? 1:0;
}


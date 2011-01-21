/* Compile-time assert-like macros.

   Copyright (C) 2005-2006, 2009-2010 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* Written by Paul Eggert, Bruno Haible, and Jim Meyering.  */

#ifndef VERIFY_H
# define VERIFY_H 1

/* Each of these macros verifies that its argument R is nonzero.  To
   be portable, R should be an integer constant expression.  Unlike
   assert (R), there is no run-time overhead.

   There are two macros, since no single macro can be used in all
   contexts in C.  verify_true (R) is for scalar contexts, including
   integer constant expression contexts.  verify (R) is for declaration
   contexts, e.g., the top level.

   Symbols ending in "__" are private to this header.

   The code below uses several ideas.

   * The first step is ((R) ? 1 : -1).  Given an expression R, of
     integral or boolean or floating-point type, this yields an
     expression of integral type, whose value is later verified to be
     constant and nonnegative.

   * Next this expression W is wrapped in a type
     struct verify_type__ { unsigned int verify_error_if_negative_size__: W; }.
     If W is negative, this yields a compile-time error.  No compiler can
     deal with a bit-field of negative size.

     One might think that an array size check would have the same
     effect, that is, that the type struct { unsigned int dummy[W]; }
     would work as well.  However, inside a function, some compilers
     (such as C++ compilers and GNU C) allow local parameters and
     variables inside array size expressions.  With these compilers,
     an array size check would not properly diagnose this misuse of
     the verify macro:

       void function (int n) { verify (n < 0); }

   * For the verify macro, the struct verify_type__ will need to
     somehow be embedded into a declaration.  To be portable, this
     declaration must declare an object, a constant, a function, or a
     typedef name.  If the declared entity uses the type directly,
     such as in

       struct dummy {...};
       typedef struct {...} dummy;
       extern struct {...} *dummy;
       extern void dummy (struct {...} *);
       extern struct {...} *dummy (void);

     two uses of the verify macro would yield colliding declarations
     if the entity names are not disambiguated.  A workaround is to
     attach the current line number to the entity name:

       #define _GL_CONCAT0(x, y) x##y
       #define _GL_CONCAT(x, y) _GL_CONCAT0 (x, y)
       extern struct {...} * _GL_CONCAT (dummy, __LINE__);

     But this has the problem that two invocations of verify from
     within the same macro would collide, since the __LINE__ value
     would be the same for both invocations.  (The GCC __COUNTER__
     macro solves this problem, but is not portable.)

     A solution is to use the sizeof operator.  It yields a number,
     getting rid of the identity of the type.  Declarations like

       extern int dummy [sizeof (struct {...})];
       extern void dummy (int [sizeof (struct {...})]);
       extern int (*dummy (void)) [sizeof (struct {...})];

     can be repeated.

   * Should the implementation use a named struct or an unnamed struct?
     Which of the following alternatives can be used?

       extern int dummy [sizeof (struct {...})];
       extern int dummy [sizeof (struct verify_type__ {...})];
       extern void dummy (int [sizeof (struct {...})]);
       extern void dummy (int [sizeof (struct verify_type__ {...})]);
       extern int (*dummy (void)) [sizeof (struct {...})];
       extern int (*dummy (void)) [sizeof (struct verify_type__ {...})];

     In the second and sixth case, the struct type is exported to the
     outer scope; two such declarations therefore collide.  GCC warns
     about the first, third, and fourth cases.  So the only remaining
     possibility is the fifth case:

       extern int (*dummy (void)) [sizeof (struct {...})];

   * GCC warns about duplicate declarations of the dummy function if
     -Wredundant_decls is used.  GCC 4.3 and later have a builtin
     __COUNTER__ macro that can let us generate unique identifiers for
     each dummy function, to suppress this warning.

   * This implementation exploits the fact that GCC does not warn about
     the last declaration mentioned above.  If a future version of GCC
     introduces a warning for this, the problem could be worked around
     by using code specialized to GCC, just as __COUNTER__ is already
     being used if available.

       #if 4 <= __GNUC__
       # define verify(R) [another version to keep GCC happy]
       #endif

   * In C++, any struct definition inside sizeof is invalid.
     Use a template type to work around the problem.  */

/* Concatenate two preprocessor tokens.  */
# define _GL_CONCAT(x, y) _GL_CONCAT0 (x, y)
# define _GL_CONCAT0(x, y) x##y

/* _GL_COUNTER is an integer, preferably one that changes each time we
   use it.  Use __COUNTER__ if it works, falling back on __LINE__
   otherwise.  __LINE__ isn't perfect, but it's better than a
   constant.  */
# if defined __COUNTER__ && __COUNTER__ != __COUNTER__
#  define _GL_COUNTER __COUNTER__
# else
#  define _GL_COUNTER __LINE__
# endif

/* Generate a symbol with the given prefix, making it unique if
   possible.  */
# define _GL_GENSYM(prefix) _GL_CONCAT (prefix, _GL_COUNTER)

/* Verify requirement R at compile-time, as an integer constant expression.
   Return 1.  */

# ifdef __cplusplus
template <int w>
  struct verify_type__ { unsigned int verify_error_if_negative_size__: w; };
#  define verify_true(R) \
     (!!sizeof (verify_type__<(R) ? 1 : -1>))
# else
#  define verify_true(R) \
     (!!sizeof \
      (struct { unsigned int verify_error_if_negative_size__: (R) ? 1 : -1; }))
# endif

/* Verify requirement R at compile-time, as a declaration without a
   trailing ';'.  */

# define verify(R) \
    extern int (* _GL_GENSYM (verify_function) (void)) [verify_true (R)]

#endif

#ifndef __CTYPE_H__
#define __CTYPE_H__

int isalnum(int c);
int isalpha(int c);
int iscntrl(int c);
int isdigit(int c);
int isgraph(int c);
int islower(int c);
int isprint(int c);
int ispunct(int c);
int isspace(int c);
int isupper(int c);
int isxdigit(int c);
int isblank(int c);
int tolower(int c);
int toupper(int c);

#if __has_builtin(__builtin_isalnum)
#   define __HAVE_BUILTIN_ISALNUM
#   define isalnum(c) __builtin_isalnum(c)

#endif


#if __has_builtin(__builtin_isalpha)
#   define __HAVE_BUILTIN_ISALPHA
#   define isalpha(c) __builtin_isalpha(c)

#endif


#if __has_builtin(__builtin_iscntrl)
#   define __HAVE_BUILTIN_ISCNTRL
#   define iscntrl(c) __builtin_iscntrl(c)

#endif


#if __has_builtin(__builtin_isdigit)
#   define __HAVE_BUILTIN_ISDIGIT
#   define isdigit(c) __builtin_isdigit(c)

#endif


#if __has_builtin(__builtin_isgraph)
#   define __HAVE_BUILTIN_ISGRAPH
#   define isgraph(c) __builtin_isgraph(c)

#endif


#if __has_builtin(__builtin_islower)
#   define __HAVE_BUILTIN_ISLOWER
#   define islower(c) __builtin_islower(c)

#endif


#if __has_builtin(__builtin_isprint)
#   define __HAVE_BUILTIN_ISPRINT
#   define isprint(c) __builtin_isprint(c)

#endif


#if __has_builtin(__builtin_ispunct)
#   define __HAVE_BUILTIN_ISPUNCT
#   define ispunct(c) __builtin_ispunct(c)

#endif


#if __has_builtin(__builtin_isspace)
#   define __HAVE_BUILTIN_ISSPACE
#   define isspace(c) __builtin_isspace(c)

#endif


#if __has_builtin(__builtin_isupper)
#   define __HAVE_BUILTIN_ISUPPER
#   define isupper(c) __builtin_isupper(c)

#endif


#if __has_builtin(__builtin_isxdigit)
#   define __HAVE_BUILTIN_ISXDIGIT
#   define isxdigit(c) __builtin_isxdigit(c)

#endif


#if __has_builtin(__builtin_isblank)
#   define __HAVE_BUILTIN_ISBLANK
#   define isablank(c) __builtin_isblank(c)

#endif


#if __has_builtin(__builtin_islower)
#   define __HAVE_BUILTIN_ISLOWER
#   define islower(c) __builtin_islower(c)

#endif


#if __has_builtin(__builtin_isupper)
#   define __HAVE_BUILTIN_ISUPPER
#   define isupper(c) __builtin_isupper(c)

#endif


#endif // __CTYPE_H__
/* Banned functions (stage 0 C dialect, cesiumlang/cesium#10), in the style
 * of git's banned.h: each name becomes a macro that expands to an undefined
 * identifier, so any use fails to compile with every compiler.
 *
 * Included last by cs_common.h, after every system header. It applies to
 * project code only.
 */
#ifndef CS_BANNED_H
#define CS_BANNED_H

#define CS_BANNED(func) sorry_##func##_is_a_banned_function

/* unbounded or non-terminating string copies */
#undef strcpy
#define strcpy(x, y) CS_BANNED(strcpy)
#undef strcat
#define strcat(x, y) CS_BANNED(strcat)
#undef strncpy
#define strncpy(x, y, n) CS_BANNED(strncpy)
#undef strncat
#define strncat(x, y, n) CS_BANNED(strncat)
#undef strlcpy
#define strlcpy(x, y, n) CS_BANNED(strlcpy)
#undef strlcat
#define strlcat(x, y, n) CS_BANNED(strlcat)

/* unbounded formatting */
#undef sprintf
#define sprintf CS_BANNED(sprintf)
#undef vsprintf
#define vsprintf CS_BANNED(vsprintf)

/* unbounded or weakly checked input */
#undef gets
#define gets(x) CS_BANNED(gets)
#undef scanf
#define scanf CS_BANNED(scanf)
#undef fscanf
#define fscanf CS_BANNED(fscanf)
#undef sscanf
#define sscanf CS_BANNED(sscanf)
#undef vscanf
#define vscanf CS_BANNED(vscanf)
#undef vfscanf
#define vfscanf CS_BANNED(vfscanf)
#undef vsscanf
#define vsscanf CS_BANNED(vsscanf)

/* number parsing with no error detection */
#undef atoi
#define atoi(x) CS_BANNED(atoi)
#undef atol
#define atol(x) CS_BANNED(atol)
#undef atoll
#define atoll(x) CS_BANNED(atoll)
#undef atof
#define atof(x) CS_BANNED(atof)

/* hidden state */
#undef strtok
#define strtok(x, y) CS_BANNED(strtok)
#undef strtok_r
#define strtok_r(x, y, z) CS_BANNED(strtok_r)

/* shared static buffers, and time makes output nondeterministic */
#undef gmtime
#define gmtime(x) CS_BANNED(gmtime)
#undef localtime
#define localtime(x) CS_BANNED(localtime)
#undef ctime
#define ctime(x) CS_BANNED(ctime)
#undef asctime
#define asctime(x) CS_BANNED(asctime)
#undef gmtime_r
#define gmtime_r(x, y) CS_BANNED(gmtime_r)
#undef localtime_r
#define localtime_r(x, y) CS_BANNED(localtime_r)
#undef ctime_r
#define ctime_r(x, y) CS_BANNED(ctime_r)
#undef asctime_r
#define asctime_r(x, y) CS_BANNED(asctime_r)
#undef time
#define time(x) CS_BANNED(time)
#undef clock
#define clock() CS_BANNED(clock)

/* race-prone temporary files */
#undef tmpnam
#define tmpnam(x) CS_BANNED(tmpnam)
#undef mktemp
#define mktemp(x) CS_BANNED(mktemp)
#undef tmpfile
#define tmpfile() CS_BANNED(tmpfile)

/* shell injection, nondeterminism, async hazards */
#undef system
#define system(x) CS_BANNED(system)
#undef getenv
#define getenv(x) CS_BANNED(getenv)
#undef signal
#define signal(x, y) CS_BANNED(signal)
#undef raise
#define raise(x) CS_BANNED(raise)

/* non-local control flow */
#undef setjmp
#define setjmp(x) CS_BANNED(setjmp)
#undef longjmp
#define longjmp(x, y) CS_BANNED(longjmp)

/* nondeterminism */
#undef rand
#define rand() CS_BANNED(rand)
#undef srand
#define srand(x) CS_BANNED(srand)

/* easy to leak or misuse on failure: growth goes through CsBuf or an arena */
#undef realloc
#define realloc(x, y) CS_BANNED(realloc)

#endif

// Variant of abbench: every measurement is the FIRST call of CompressMem in a fresh child
// process (cold caches, zeroed globals and fresh allocator, as in a real `arc` run), which
// matters for codecs with global state such as DICT. Pairs alternate the base/cand order and
// the verdict uses the same paired log-ratio confidence interval as abbench.
//
// Usage: abfirst BASE.so CAND.so METHOD PAIRS FILE...
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <sys/wait.h>
#include <vector>
#include <algorithm>
typedef int (*CompressMemFn) (char *, void *, int, void *, int);
static double now() { timespec t; clock_gettime (CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
static std::vector<char> in;
static double measure (const char *so, const char *method)
{
    int fd[2]; pipe (fd);
    pid_t pid = fork();
    if (pid == 0) {
        void *h = dlopen (so, RTLD_NOW|RTLD_LOCAL); CompressMemFn f = (CompressMemFn) dlsym (h, "CompressMem");
        int cap = in.size() + in.size()/2 + (1<<20); char *out = (char*) malloc (cap); memset (out, 0, cap);
        char m[256]; strcpy (m, method);
        double t0 = now(); int n = f (m, &in[0], in.size(), out, cap); double t = now() - t0;
        if (n < 0) t = -1;
        write (fd[1], &t, sizeof t); _exit (0);
    }
    double t; read (fd[0], &t, sizeof t); waitpid (pid, 0, 0); close (fd[0]); close (fd[1]);
    return t;
}
int main (int argc, char **argv)
{
    int pairs = atoi (argv[4]);
    for (int i = 5; i < argc; i++) { FILE *f = fopen (argv[i], "rb"); fseek (f,0,SEEK_END); long n = ftell (f); fseek (f,0,SEEK_SET); size_t o = in.size(); in.resize (o+n); fread (&in[o],1,n,f); fclose (f); }
    std::vector<double> r, tb, tc;
    for (int p = 0; p < pairs; p++) {
        double b, c;
        if (p & 1) { c = measure (argv[2], argv[3]); b = measure (argv[1], argv[3]); }
        else       { b = measure (argv[1], argv[3]); c = measure (argv[2], argv[3]); }
        tb.push_back (b); tc.push_back (c); r.push_back (log (c/b));
    }
    double s=0,s2=0; for (double x: r) {s+=x; s2+=x*x;} double mean=s/pairs, sd=sqrt((s2-s*s/pairs)/(pairs-1)), ci=1.96*sd/sqrt(pairs);
    int wins=0; for (double x: r) wins += x<0;
    std::sort (tb.begin(), tb.end()); std::sort (tc.begin(), tc.end());
    double g=(1-exp(mean))*100, lo=(1-exp(mean+ci))*100, hi=(1-exp(mean-ci))*100;
    printf ("%-34s base %.4f cand %.4f gain %6.2f%% [%6.2f%% ..%6.2f%%] wins %d/%d %s\n", argv[3], tb[0], tc[0], g, lo, hi, wins, pairs, g>1&&lo>0.5?"GAIN": g<-1&&hi<-0.5?"LOSS":"noise");
}

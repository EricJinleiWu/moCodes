/*
    Algorithm description:
    1.get prime number p, q;
    2.get N : N = p * q;
    3.get L : L = (p - 1) * (q - 1);
    4.get E : 
        gcd(E, L) = 1;
        1 < E < L;
      Currently, we use 11 or 17 as E;
    5.get D:
        1 < D < L;
        E * D % L = 1;
            This means, E * D - 1 = k * L;
            So, we just need to calc : E * D + K * L = 1;
            This should be done by extend euclid algorithm;

    (E, N)is public key, (D, N) is private key;
*/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

//Cannot too large, because we just can deal with little number now, large number will be done later.
#define MAX_PRIME_NUM   (30)

#define E_VALUE1    11
#define E_VALUE2    17
#define E_VALUE3    65537

/*
    return 0 if not prime number, return 1 else;
*/
static int isPrimeNumber(const int num)
{
    if(num < 2)
    {
        return 0;
    }
    
    if(num % 2 == 0)
    {
        return 0;
    }

    int s = (int)sqrt(num) + 1;
    int i = 1;
    for(i = 2; i < s; i++)
    {
        if(num % i == 0)
            return 0;
        else 
            continue;
    }
    return 1;
}

/*
    Get a random number, which should be a prime number;
*/
static int getRandPrimeNum(void)
{
    int ret = rand() % MAX_PRIME_NUM + 5;
    while(!isPrimeNumber(ret))
    {
        ret++;
    }
    return ret;
}

/*
    gcd
*/
static int gcd(const int num1, const int num2)
{
    int a = 0, b = 0, c = 0;
    a = num1;
    b = num2;
    while( (c = a % b) != 0)
    {
        a = b;
        b = c;
    }
    return c;
}

/*
    lcm, get gcd() first, then get lcm
*/
static int lcm(const int num1, const int num2)
{
    int ret = num1 * num2 / gcd(num1, num2);
    return ret;
}

/*
    To public key [E, N], get E here.
*/
static int getE()
{
    return 11;
}

static int extendEuclid(const int E, const int L, int * D, int * k)
{
    if(L==0)
    {
        *D = 1;
        *k = 0;
        return E;
    }
    
    int r = extendEuclid(L, E % L, D, k);
    int t = *D;
    *D = *k;
    *k = t - E / L * (*k);

    return r;
}

int exGcd(int a,int b,int &x,int &y)
{
    if(b==0)
    {
        x=1;
        y=0;
        return a;
    }
    int r=exGcd(b,a%b,x,y);
    int t=x;
    x=y;
    y=t-a/b*y;
    
    return r;
}

#if 0
int main(int argc, char **argv)
{
    int E = 11, L = 120;
    int D = 0, K = 0;
    extendEuclid(E, L, &D, &K);
    printf("E = %d, L = %d, d = %d, k = %d\n", E, L, D, K);

    exGcd(E, L, D, K);
    printf("E = %d, L = %d, d = %d, k = %d\n", E, L, D, K);
    
    return 0;
}
#endif

/*
    E * d + k * L = 1;
    this can be calced by extend euclid algorithm.
*/
static int getD(const int E, const int L)
{
    int d = 0, k = 0;
    int ret = extendEuclid(E, L, &d, &k);
    printf("wjl_test : ret = %d, E = %d, L = %d, d = %d, k = %d\n", ret, E, L, d, k);
    
    return d;
}

static int genKeys(int *E, int *D, int *N)
{
    int p = getRandPrimeNum();
    int q = getRandPrimeNum();
    p = 11;
    q = 13;
    printf("p = %d, q = %d\n", p, q);

    int L = (p - 1) * (q - 1);
    *N = p * q;
    *E = getE();
    *D = getD(*E, L);
    printf("E = %d, D = %d, N = %d, L = %d\n", *E, *D, *N, L);

	//This is very important!!
    if(*D < 0)
    {
        *D = *D + L;
        printf("D < 0, we convert it to %d\n", D);
    }
    
    return 0;
}

/*
    src ** ED % N;
*/
static int rsaCrypt(const int ED, const int N, const int src, int * dst)
{
    int ret = 0;
    int i = 0; 
    int modRet = 1;
    for(i = 0; i < ED; i++)
    {
        ret = (src * modRet) % N;
        modRet = ret;
    }
    *dst = modRet;
    
    return 0;
}

int main(int argc, char **argv)
{
    srand((unsigned int )time(NULL));
    
    int E = 0, D = 0, N = 0;
    
    printf("do you need a new keys? Input y or n : ");
    char in[8] = {0x00};
    while(1)
    {
        memset(in, 0x00, 8);
        scanf("%s", in);
        printf("\n");
        in[7] = 0x00;
        if(strcmp(in, "y") == 0)
        {
            genKeys(&E, &D, &N);
            printf("public key is [%d, %d], private key is [%d, %d]\n",
                E, N, D, N);
            break;
        }
        else if(strcmp(in, "n") == 0)
        {
            break;
        }
        else
        {
            printf("Input value [%s] donot supported! just support \"y\" or \"n\" currently.\n", in);
            continue;
        }
    }

    //Start do encrypt and decrypt
    int src = rand() % 64 + 16;
    int encDst = 0, decDst = 0;
    rsaCrypt(E, N, src, &encDst);
    rsaCrypt(D, N, encDst, &decDst);
    printf("src = %d, encDst = %d, decDst = %d\n", src, encDst, decDst);
    
    return 0;
}

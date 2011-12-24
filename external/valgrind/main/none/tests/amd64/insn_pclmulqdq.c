#include <math.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

typedef union {
  char sb[1];
  unsigned char ub[1];
} reg8_t;

typedef union {
  char sb[2];
  unsigned char ub[2];
  short sw[1];
  unsigned short uw[1];
} reg16_t;

typedef union {
  char sb[4];
  unsigned char ub[4];
  short sw[2];
  unsigned short uw[2];
  int sd[1];
  unsigned int ud[1];
  float ps[1];
} reg32_t;

typedef union {
  char sb[8];
  unsigned char ub[8];
  short sw[4];
  unsigned short uw[4];
  int sd[2];
  unsigned int ud[2];
  long long int sq[1];
  unsigned long long int uq[1];
  float ps[2];
  double pd[1];
} reg64_t __attribute__ ((aligned (8)));

typedef union {
  char sb[16];
  unsigned char ub[16];
  short sw[8];
  unsigned short uw[8];
  int sd[4];
  unsigned int ud[4];
  long long int sq[2];
  unsigned long long int uq[2];
  float ps[4];
  double pd[2];
} reg128_t __attribute__ ((aligned (16)));

static sigjmp_buf catchpoint;

static void handle_sigill(int signum)
{
   siglongjmp(catchpoint, 1);
}

__attribute__((unused))
static int eq_float(float f1, float f2)
{
   /* return f1 == f2 || fabsf(f1 - f2) < fabsf(f1) * 1.5 * powf(2,-12); */
   return f1 == f2 || fabsf(f1 - f2) < fabsf(f1) * 1.5 / 4096.0;
}

__attribute__((unused))
static int eq_double(double d1, double d2)
{
   /* return d1 == d2 || fabs(d1 - d2) < fabs(d1) * 1.5 * pow(2,-12); */
   return d1 == d2 || fabs(d1 - d2) < fabs(d1) * 1.5 / 4096.0;
}

static void pclmulqdq_1(void)
{
   reg128_t arg1 = { .uq = { 0x00017004200ab0cdULL, 0xc000b802f6b31753ULL } };
   reg128_t arg2 = { .uq = { 0xa0005c0252074a9aULL, 0x50002e0207b1643cULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $0, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x5ff61cc8b1043fa2ULL && result0.uq[1] == 0x00009602d147dc12ULL )
      {
         printf("pclmulqdq_1 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_1 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x5ff61cc8b1043fa2ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x00009602d147dc12ULL);
      }
   }
   else
   {
      printf("pclmulqdq_1 ... failed\n");
   }

   return;
}

static void pclmulqdq_2(void)
{
   reg128_t arg1 = { .uq = { 0x28001701e286710dULL, 0xd4000b81d7f0f773ULL } };
   reg128_t arg2 = { .uq = { 0xaa0005c1c2a63aaaULL, 0x550002e1c000dc44ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $1, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0xd33d2883021ccb74ULL && result0.uq[1] == 0x080804b056c3c3bdULL )
      {
         printf("pclmulqdq_2 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_2 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0xd33d2883021ccb74ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x080804b056c3c3bdULL);
      }
   }
   else
   {
      printf("pclmulqdq_2 ... failed\n");
   }

   return;
}

static void pclmulqdq_3(void)
{
   reg128_t arg1 = { .uq = { 0x2a800171beae2d11ULL, 0xd54000b9b604d579ULL } };
   reg128_t arg2 = { .uq = { 0xaaa0005db1b029adULL, 0x9550002faf85d3c3ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $16, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x5bd93710a920a9f5ULL && result0.uq[1] == 0x777888724b473f64ULL )
      {
         printf("pclmulqdq_3 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_3 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x5bd93710a920a9f5ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x777888724b473f64ULL);
      }
   }
   else
   {
      printf("pclmulqdq_3 ... failed\n");
   }

   return;
}

static void pclmulqdq_4(void)
{
   reg128_t arg1 = { .uq = { 0x8aa80018be70a8d2ULL, 0x4554000d3de61358ULL } };
   reg128_t arg2 = { .uq = { 0x22aa00077da0c89bULL, 0xd1550004957e233eULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $17, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0xd222922d28094790ULL && result0.uq[1] == 0x37fb44403e2d3407ULL )
      {
         printf("pclmulqdq_4 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_4 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0xd222922d28094790ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x37fb44403e2d3407ULL);
      }
   }
   else
   {
      printf("pclmulqdq_4 ... failed\n");
   }

   return;
}

static void pclmulqdq_5(void)
{
   reg128_t arg1 = { .uq = { 0x68aa8003296cd08eULL, 0x3455400273642736ULL } };
   reg128_t arg2 = { .uq = { 0x1a2aa002185fd28aULL, 0x0d155001eadda834ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $0, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x6f56f9abeba01e6cULL && result0.uq[1] == 0x05101111e9709d8fULL )
      {
         printf("pclmulqdq_5 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_5 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x6f56f9abeba01e6cULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x05101111e9709d8fULL);
      }
   }
   else
   {
      printf("pclmulqdq_5 ... failed\n");
   }

   return;
}

static void pclmulqdq_6(void)
{
   reg128_t arg1 = { .uq = { 0x068aa801d41c9309ULL, 0xc3455401c0bc0875ULL } };
   reg128_t arg2 = { .uq = { 0xa1a2aa01c70bc327ULL, 0x90d15501ca33a080ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $1, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x0c18b0e8ab072480ULL && result0.uq[1] == 0x032f76887b10d528ULL )
      {
         printf("pclmulqdq_6 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_6 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x0c18b0e8ab072480ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x032f76887b10d528ULL);
      }
   }
   else
   {
      printf("pclmulqdq_6 ... failed\n");
   }

   return;
}

static void pclmulqdq_7(void)
{
   reg128_t arg1 = { .uq = { 0x4868aa81c3c78f2fULL, 0xe4345541c8918684ULL } };
   reg128_t arg2 = { .uq = { 0x721a2aa1c2f68231ULL, 0xf90d1551c8290009ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $16, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x11d8b7b8f72e3644ULL && result0.uq[1] == 0x2a080288f207712bULL )
      {
         printf("pclmulqdq_7 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_7 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x11d8b7b8f72e3644ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x2a080288f207712bULL);
      }
   }
   else
   {
      printf("pclmulqdq_7 ... failed\n");
   }

   return;
}

static void pclmulqdq_8(void)
{
   reg128_t arg1 = { .uq = { 0xbc868aa9cac23ef5ULL, 0x9e434555cc0ede67ULL } };
   reg128_t arg2 = { .uq = { 0x8f21a2abccb52e20ULL, 0x4790d156c50855ffULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $17, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0xd2e5bdd1665023ddULL && result0.uq[1] == 0x240dbdff7a0eb888ULL )
      {
         printf("pclmulqdq_8 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_8 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0xd2e5bdd1665023ddULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x240dbdff7a0eb888ULL);
      }
   }
   else
   {
      printf("pclmulqdq_8 ... failed\n");
   }

   return;
}

static void pclmulqdq_9(void)
{
   reg128_t arg1 = { .uq = { 0xe3c868ac4931e9ecULL, 0x71e434570346b3e5ULL } };
   reg128_t arg2 = { .uq = { 0xf8f21a2c685118dfULL, 0xbc790d171ad64b5cULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $0, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x0eebfc038c776124ULL && result0.uq[1] == 0x5c177a6fb4d9adf2ULL )
      {
         printf("pclmulqdq_9 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_9 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x0eebfc038c776124ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x5c177a6fb4d9adf2ULL);
      }
   }
   else
   {
      printf("pclmulqdq_9 ... failed\n");
   }

   return;
}

static void pclmulqdq_10(void)
{
   reg128_t arg1 = { .uq = { 0x5e3c868c6c18e49dULL, 0xef1e43471cba313bULL } };
   reg128_t arg2 = { .uq = { 0xb78f21a4650ad78eULL, 0x5bc790d311332ab6ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $1, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x01f223ce761bbdbeULL && result0.uq[1] == 0x1046696140e99a8dULL )
      {
         printf("pclmulqdq_10 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_10 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x01f223ce761bbdbeULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x1046696140e99a8dULL);
      }
   }
   else
   {
      printf("pclmulqdq_10 ... failed\n");
   }

   return;
}

static void pclmulqdq_11(void)
{
   reg128_t arg1 = { .uq = { 0x2de3c86a6747544aULL, 0x16f1e43612516914ULL } };
   reg128_t arg2 = { .uq = { 0x0b78f21be7d67379ULL, 0xc5bc790eda98f8adULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $16, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0xf1b07b5d2dce2b74ULL && result0.uq[1] == 0x008a2a80a6dea4c8ULL )
      {
         printf("pclmulqdq_11 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_11 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0xf1b07b5d2dce2b74ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x008a2a80a6dea4c8ULL);
      }
   }
   else
   {
      printf("pclmulqdq_11 ... failed\n");
   }

   return;
}

static void pclmulqdq_12(void)
{
   reg128_t arg1 = { .uq = { 0xa2de3c8843fa3b43ULL, 0x916f1e4508aadc92ULL } };
   reg128_t arg2 = { .uq = { 0x48b78f2363032d38ULL, 0x245bc792902f558bULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $17, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0xf8b35e3453aab226ULL && result0.uq[1] == 0x10404514973eeacdULL )
      {
         printf("pclmulqdq_12 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_12 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0xf8b35e3453aab226ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x10404514973eeacdULL);
      }
   }
   else
   {
      printf("pclmulqdq_12 ... failed\n");
   }

   return;
}

static void pclmulqdq_13(void)
{
   reg128_t arg1 = { .uq = { 0xd22de3ca1ec569b6ULL, 0x6916f1e5ee1073caULL } };
   reg128_t arg2 = { .uq = { 0x348b78f3d5b5f8d4ULL, 0x1a45bc7ac988bb59ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $0, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x6e44f4974d351b38ULL && result0.uq[1] == 0x14410114bf2270eaULL )
      {
         printf("pclmulqdq_13 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_13 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x6e44f4974d351b38ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x14410114bf2270eaULL);
      }
   }
   else
   {
      printf("pclmulqdq_13 ... failed\n");
   }

   return;
}

static void pclmulqdq_14(void)
{
   reg128_t arg1 = { .uq = { 0xcd22de3e4b721c9dULL, 0xa6916f200c66cd3bULL } };
   reg128_t arg2 = { .uq = { 0x9348b790ece1258eULL, 0x49a45bc9551e51b6ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $1, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0xd9ebb20510c452beULL && result0.uq[1] == 0x3590bae854a1ffd5ULL )
      {
         printf("pclmulqdq_14 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_14 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0xd9ebb20510c452beULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x3590bae854a1ffd5ULL);
      }
   }
   else
   {
      printf("pclmulqdq_14 ... failed\n");
   }

   return;
}

static void pclmulqdq_15(void)
{
   reg128_t arg1 = { .uq = { 0x24d22de5893ce7caULL, 0x126916f3a34c32d4ULL } };
   reg128_t arg2 = { .uq = { 0x09348b7ab053d859ULL, 0xc49a45be2ed7ab1dULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $16, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0xeb5e5f29e6badc34ULL && result0.uq[1] == 0x00820a20b0fa8cedULL )
      {
         printf("pclmulqdq_15 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_15 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0xeb5e5f29e6badc34ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x00820a20b0fa8cedULL);
      }
   }
   else
   {
      printf("pclmulqdq_15 ... failed\n");
   }

   return;
}

static void pclmulqdq_16(void)
{
   reg128_t arg1 = { .uq = { 0xa24d22dffe19947bULL, 0x91269170d5ba892eULL } };
   reg128_t arg2 = { .uq = { 0x489348b9498b0386ULL, 0x2449a45d837340b2ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $17, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x1934a4ec2d51b27cULL && result0.uq[1] == 0x10404105d1aac198ULL )
      {
         printf("pclmulqdq_16 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_16 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x1934a4ec2d51b27cULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x10404105d1aac198ULL);
      }
   }
   else
   {
      printf("pclmulqdq_16 ... failed\n");
   }

   return;
}

static void pclmulqdq_17(void)
{
   reg128_t arg1 = { .uq = { 0x1224d22fa0675f48ULL, 0x09126918aee16e93ULL } };
   reg128_t arg2 = { .uq = { 0xc489348d3e1e763aULL, 0x62449a477dbcfa0cULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $0, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x1411baeeee166950ULL && result0.uq[1] == 0x0dda5c984c642a65ULL )
      {
         printf("pclmulqdq_17 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_17 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x1411baeeee166950ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x0dda5c984c642a65ULL);
      }
   }
   else
   {
      printf("pclmulqdq_17 ... failed\n");
   }

   return;
}

static void pclmulqdq_18(void)
{
   reg128_t arg1 = { .uq = { 0x31224d249d8c3bf5ULL, 0xd89126932573dce7ULL } };
   reg128_t arg2 = { .uq = { 0xac48934a7967ad60ULL, 0x562449a61b61959fULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $1, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0xf21b031e1f9fc8b3ULL && result0.uq[1] == 0x0ffa971b97fa8b81ULL )
      {
         printf("pclmulqdq_18 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_18 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0xf21b031e1f9fc8b3ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x0ffa971b97fa8b81ULL);
      }
   }
   else
   {
      printf("pclmulqdq_18 ... failed\n");
   }

   return;
}

static void pclmulqdq_19(void)
{
   reg128_t arg1 = { .uq = { 0xeb1224d3e45e89bcULL, 0x7589126ad0dd03cdULL } };
   reg128_t arg2 = { .uq = { 0xfac489363f1c40d3ULL, 0xbd62449bf63bdf5aULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $16, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x751e203b33096d47ULL && result0.uq[1] == 0x2d2e6d8fd926d075ULL )
      {
         printf("pclmulqdq_19 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_19 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x751e203b33096d47ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x2d2e6d8fd926d075ULL);
      }
   }
   else
   {
      printf("pclmulqdq_19 ... failed\n");
   }

   return;
}

static void pclmulqdq_20(void)
{
   reg128_t arg1 = { .uq = { 0x5eb1224ed9cbae9cULL, 0x2f5891284b93963dULL } };
   reg128_t arg2 = { .uq = { 0xd7ac48950c778a0bULL, 0xabd6244b6ce983f6ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $17, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x6c7f0f43ad1d863eULL && result0.uq[1] == 0x13521ee11ef3275eULL )
      {
         printf("pclmulqdq_20 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_20 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x6c7f0f43ad1d863eULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x13521ee11ef3275eULL);
      }
   }
   else
   {
      printf("pclmulqdq_20 ... failed\n");
   }

   return;
}

static void pclmulqdq_21(void)
{
   reg128_t arg1 = { .uq = { 0x55eb1226952280eaULL, 0x2af58914293eff64ULL } };
   reg128_t arg2 = { .uq = { 0x157ac48af34d3ea1ULL, 0xcabd624650545e41ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $0, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x249f99dae3b624aaULL && result0.uq[1] == 0x04445511afa5163bULL )
      {
         printf("pclmulqdq_21 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_21 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x249f99dae3b624aaULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x04445511afa5163bULL);
      }
   }
   else
   {
      printf("pclmulqdq_21 ... failed\n");
   }

   return;
}

static void pclmulqdq_22(void)
{
   reg128_t arg1 = { .uq = { 0xa55eb123fed7ee11ULL, 0x92af5892d619b5f9ULL } };
   reg128_t arg2 = { .uq = { 0x8957ac4a41ba99edULL, 0x84abd626078b0be3ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $1, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x98f8fc12fdf0c7d3ULL && result0.uq[1] == 0x507891a8303b6e0dULL )
      {
         printf("pclmulqdq_22 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_22 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x98f8fc12fdf0c7d3ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x507891a8303b6e0dULL);
      }
   }
   else
   {
      printf("pclmulqdq_22 ... failed\n");
   }

   return;
}

static void pclmulqdq_23(void)
{
   reg128_t arg1 = { .uq = { 0x8255eb13ea7344e2ULL, 0x412af58ad3e76160ULL } };
   reg128_t arg2 = { .uq = { 0x20957ac648a16f9fULL, 0xd04abd640afe76bcULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $16, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x5d4e25af2f70ab20ULL && result0.uq[1] == 0x08008222e18727b6ULL )
      {
         printf("pclmulqdq_23 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_23 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x5d4e25af2f70ab20ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x08008222e18727b6ULL);
      }
   }
   else
   {
      printf("pclmulqdq_23 ... failed\n");
   }

   return;
}

static void pclmulqdq_24(void)
{
   reg128_t arg1 = { .uq = { 0x68255eb2e42cfa4dULL, 0xf412af5a58c43c13ULL } };
   reg128_t arg2 = { .uq = { 0xba0957ae030fdcfaULL, 0x5d04abd7e035ad6cULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $17, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0xa2f470774f11b174ULL && result0.uq[1] == 0x36c2fe7c16d26dabULL )
      {
         printf("pclmulqdq_24 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_24 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0xa2f470774f11b174ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x36c2fe7c16d26dabULL);
      }
   }
   else
   {
      printf("pclmulqdq_24 ... failed\n");
   }

   return;
}

static void pclmulqdq_25(void)
{
   reg128_t arg1 = { .uq = { 0x2e8255eccec895a5ULL, 0xd7412af74e1209bfULL } };
   reg128_t arg2 = { .uq = { 0xaba0957c8db6c3ccULL, 0x55d04abf258920d5ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $0, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0xc3db9ad675a26f7cULL && result0.uq[1] == 0x1384704a229c5d7fULL )
      {
         printf("pclmulqdq_25 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_25 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0xc3db9ad675a26f7cULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x1384704a229c5d7fULL);
      }
   }
   else
   {
      printf("pclmulqdq_25 ... failed\n");
   }

   return;
}

static void pclmulqdq_26(void)
{
   reg128_t arg1 = { .uq = { 0xeae8256079724f57ULL, 0xb57412b11366e698ULL } };
   reg128_t arg2 = { .uq = { 0x5aba09596861323bULL, 0xed5d04ad9ade580eULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $1, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x631d20044d9fd14aULL && result0.uq[1] == 0x56b5179ab8f1355fULL )
      {
         printf("pclmulqdq_26 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_26 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x631d20044d9fd14aULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x56b5179ab8f1355fULL);
      }
   }
   else
   {
      printf("pclmulqdq_26 ... failed\n");
   }

   return;
}

static void pclmulqdq_27(void)
{
   reg128_t arg1 = { .uq = { 0x76ae8257ac1ceaf6ULL, 0x3b57412cb4bc346aULL } };
   reg128_t arg2 = { .uq = { 0x1daba097390bd924ULL, 0x0ed5d04c7b33ab81ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $16, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x43140ac0e96646e8ULL && result0.uq[1] == 0x02a2888abaa8a737ULL )
      {
         printf("pclmulqdq_27 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_27 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x43140ac0e96646e8ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x02a2888abaa8a737ULL);
      }
   }
   else
   {
      printf("pclmulqdq_27 ... failed\n");
   }

   return;
}

static void pclmulqdq_28(void)
{
   reg128_t arg1 = { .uq = { 0xc76ae827144794b1ULL, 0xa3b5741460d18949ULL } };
   reg128_t arg2 = { .uq = { 0x91daba0b17168395ULL, 0x88ed5d06623900b7ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $17, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x84dbc63cd168a7cfULL && result0.uq[1] == 0x54ad45cd2f140103ULL )
      {
         printf("pclmulqdq_28 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_28 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x84dbc63cd168a7cfULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x54ad45cd2f140103ULL);
      }
   }
   else
   {
      printf("pclmulqdq_28 ... failed\n");
   }

   return;
}

static void pclmulqdq_29(void)
{
   reg128_t arg1 = { .uq = { 0x8476ae8417ca3f48ULL, 0x423b5742ea92de93ULL } };
   reg128_t arg2 = { .uq = { 0xe11daba25bf72e3aULL, 0x708ed5d20ca9560cULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $0, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x2f6fd6d371c96950ULL && result0.uq[1] == 0x7322f9a7bdfbf7ceULL )
      {
         printf("pclmulqdq_29 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_29 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x2f6fd6d371c96950ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x7322f9a7bdfbf7ceULL);
      }
   }
   else
   {
      printf("pclmulqdq_29 ... failed\n");
   }

   return;
}

static void pclmulqdq_30(void)
{
   reg128_t arg1 = { .uq = { 0x38476ae9e50269f5ULL, 0xdc23b575d92ef3e7ULL } };
   reg128_t arg2 = { .uq = { 0xae11dabbc34538e0ULL, 0x5708ed5ec0505b5fULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $1, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x36ab4b9f08a71773ULL && result0.uq[1] == 0x0d3dae161adae679ULL )
      {
         printf("pclmulqdq_30 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_30 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x36ab4b9f08a71773ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x0d3dae161adae679ULL);
      }
   }
   else
   {
      printf("pclmulqdq_30 ... failed\n");
   }

   return;
}

static void pclmulqdq_31(void)
{
   reg128_t arg1 = { .uq = { 0xeb8476b046d5ec9cULL, 0x75c23b590218b53dULL } };
   reg128_t arg2 = { .uq = { 0xfae11dad67ba198bULL, 0xbd708ed79a8acbb6ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $16, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x90ab3040b69bed2fULL && result0.uq[1] == 0x2d193b789ecdb8bfULL )
      {
         printf("pclmulqdq_31 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_31 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x90ab3040b69bed2fULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x2d193b789ecdb8bfULL);
      }
   }
   else
   {
      printf("pclmulqdq_31 ... failed\n");
   }

   return;
}

static void pclmulqdq_32(void)
{
   reg128_t arg1 = { .uq = { 0x5eb8476cabf324caULL, 0x2f5c23b734a75154ULL } };
   reg128_t arg2 = { .uq = { 0x17ae11dc79016799ULL, 0xcbd708ef132e72bdULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $17, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x58394f2a30276364ULL && result0.uq[1] == 0x1d6c5d6217d64627ULL )
      {
         printf("pclmulqdq_32 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_32 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x58394f2a30276364ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x1d6c5d6217d64627ULL);
      }
   }
   else
   {
      printf("pclmulqdq_32 ... failed\n");
   }

   return;
}

static void pclmulqdq_33(void)
{
   reg128_t arg1 = { .uq = { 0xa5eb84786044f84bULL, 0x92f5c23d16d03b16ULL } };
   reg128_t arg2 = { .uq = { 0x497ae11f6a15dc7aULL, 0x24bd709093b8ad2cULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $0, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0xa19f5f3d150729deULL && result0.uq[1] == 0x2cc3c480d7262702ULL )
      {
         printf("pclmulqdq_33 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_33 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0xa19f5f3d150729deULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x2cc3c480d7262702ULL);
      }
   }
   else
   {
      printf("pclmulqdq_33 ... failed\n");
   }

   return;
}

static void pclmulqdq_34(void)
{
   reg128_t arg1 = { .uq = { 0x125eb849288a1585ULL, 0xc92f5c257af2c9afULL } };
   reg128_t arg2 = { .uq = { 0xa497ae13942723c4ULL, 0x524bd70aa8c150d1ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $1, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0xd86609565dd8fe15ULL && result0.uq[1] == 0x0592c7bc6f0bff4bULL )
      {
         printf("pclmulqdq_34 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_34 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0xd86609565dd8fe15ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x0592c7bc6f0bff4bULL);
      }
   }
   else
   {
      printf("pclmulqdq_34 ... failed\n");
   }

   return;
}

static void pclmulqdq_35(void)
{
   reg128_t arg1 = { .uq = { 0xe925eb863b0e6759ULL, 0xb492f5c3f434f29dULL } };
   reg128_t arg2 = { .uq = { 0x9a497ae2d0c8383bULL, 0x8d24bd723f11db0eULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $16, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x5a7c0b0663ed613fULL && result0.uq[1] == 0x55e5e712e20a7f3dULL )
      {
         printf("pclmulqdq_35 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_35 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x5a7c0b0663ed613fULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x55e5e712e20a7f3dULL);
      }
   }
   else
   {
      printf("pclmulqdq_35 ... failed\n");
   }

   return;
}

static void pclmulqdq_36(void)
{
   reg128_t arg1 = { .uq = { 0x46925eb9fe36ac76ULL, 0x23492f5dddc9152aULL } };
   reg128_t arg2 = { .uq = { 0x11a497afcd924984ULL, 0x08d24bd8c576e3b1ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $17, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x64d528322b29c9caULL && result0.uq[1] == 0x01014411a41cd8f9ULL )
      {
         printf("pclmulqdq_36 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_36 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x64d528322b29c9caULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x01014411a41cd8f9ULL);
      }
   }
   else
   {
      printf("pclmulqdq_36 ... failed\n");
   }

   return;
}

static void pclmulqdq_37(void)
{
   reg128_t arg1 = { .uq = { 0xc46925ed496930c9ULL, 0xa23492f78b625755ULL } };
   reg128_t arg2 = { .uq = { 0x911a497cac5eea97ULL, 0x888d24bf3cdd3438ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $0, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x9ee57eac6392c06fULL && result0.uq[1] == 0x6ebdb35952de298bULL )
      {
         printf("pclmulqdq_37 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_37 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x9ee57eac6392c06fULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x6ebdb35952de298bULL);
      }
   }
   else
   {
      printf("pclmulqdq_37 ... failed\n");
   }

   return;
}

static void pclmulqdq_38(void)
{
   reg128_t arg1 = { .uq = { 0x444692607d1c590bULL, 0xe2234931153beb76ULL } };
   reg128_t arg2 = { .uq = { 0x7111a499694bb4aaULL, 0x3888d24d93539944ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $1, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0xe76d06e2b08e45ecULL && result0.uq[1] == 0x0eceb968e17faa1fULL )
      {
         printf("pclmulqdq_38 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_38 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0xe76d06e2b08e45ecULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x0eceb968e17faa1fULL);
      }
   }
   else
   {
      printf("pclmulqdq_38 ... failed\n");
   }

   return;
}

static void pclmulqdq_39(void)
{
   reg128_t arg1 = { .uq = { 0x1c446927a8578b91ULL, 0xce223494bad984b9ULL } };
   reg128_t arg2 = { .uq = { 0xa7111a4b341a814dULL, 0x93888d2670baff93ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $16, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x351bc2f119e0a4d5ULL && result0.uq[1] == 0x7cb3956d93a777bbULL )
      {
         printf("pclmulqdq_39 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_39 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x351bc2f119e0a4d5ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x7cb3956d93a777bbULL);
      }
   }
   else
   {
      printf("pclmulqdq_39 ... failed\n");
   }

   return;
}

static void pclmulqdq_40(void)
{
   reg128_t arg1 = { .uq = { 0x89c446940f0b3ebaULL, 0x44e2234ae6335e4cULL } };
   reg128_t arg2 = { .uq = { 0x227111a651c76e15ULL, 0xd13888d3ff9175f7ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $17, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x52b883d7adab3fa4ULL && result0.uq[1] == 0x374d8c769fd3cfd2ULL )
      {
         printf("pclmulqdq_40 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_40 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x52b883d7adab3fa4ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x374d8c769fd3cfd2ULL);
      }
   }
   else
   {
      printf("pclmulqdq_40 ... failed\n");
   }

   return;
}

static void pclmulqdq_41(void)
{
   reg128_t arg1 = { .uq = { 0xa89c446ad67679e8ULL, 0x544e223649e8fbe3ULL } };
   reg128_t arg2 = { .uq = { 0xea27111c0ba23ce2ULL, 0x7513888ee47edd60ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $0, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x71258e2844da20d0ULL && result0.uq[1] == 0x6f79237864913e89ULL )
      {
         printf("pclmulqdq_41 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_41 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x71258e2844da20d0ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x6f79237864913e89ULL);
      }
   }
   else
   {
      printf("pclmulqdq_41 ... failed\n");
   }

   return;
}

static void pclmulqdq_42(void)
{
   reg128_t arg1 = { .uq = { 0x3a89c44850ed2d9fULL, 0xdd44e224ff2455bcULL } };
   reg128_t arg2 = { .uq = { 0x6ea271135e3fe9cdULL, 0xf751388a85cdb3d3ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $1, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x0dce470a58b03611ULL && result0.uq[1] == 0x17b7bff3cae3b878ULL )
      {
         printf("pclmulqdq_42 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_42 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x0dce470a58b03611ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x17b7bff3cae3b878ULL);
      }
   }
   else
   {
      printf("pclmulqdq_42 ... failed\n");
   }

   return;
}

static void pclmulqdq_43(void)
{
   reg128_t arg1 = { .uq = { 0xbba89c46299498daULL, 0x5dd44e23f3780b5cULL } };
   reg128_t arg2 = { .uq = { 0x2eea2712d869c49dULL, 0xd775138a42e2a13bULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $16, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x8e66578f04b5170cULL && result0.uq[1] == 0x08a8a888e58cdcd5ULL )
      {
         printf("pclmulqdq_43 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_43 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x8e66578f04b5170cULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x08a8a888e58cdcd5ULL);
      }
   }
   else
   {
      printf("pclmulqdq_43 ... failed\n");
   }

   return;
}

static void pclmulqdq_44(void)
{
   reg128_t arg1 = { .uq = { 0xabba89c6081f0f8eULL, 0x55dd44e3e2bd46b6ULL } };
   reg128_t arg2 = { .uq = { 0x2aeea272d00c624aULL, 0x1577513a46b3f014ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $17, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x00039e62361051b8ULL && result0.uq[1] == 0x04445454c9e2e3b4ULL )
      {
         printf("pclmulqdq_44 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_44 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x00039e62361051b8ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x04445454c9e2e3b4ULL);
      }
   }
   else
   {
      printf("pclmulqdq_44 ... failed\n");
   }

   return;
}

static void pclmulqdq_45(void)
{
   reg128_t arg1 = { .uq = { 0x0abba89e0207b6f9ULL, 0xc55dd44fe7b19a6dULL } };
   reg128_t arg2 = { .uq = { 0xa2aeea28da868c23ULL, 0x9157751543f10502ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $0, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0xec91a12c1e78a82bULL && result0.uq[1] == 0x041bb00df2e9eeb3ULL )
      {
         printf("pclmulqdq_45 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_45 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0xec91a12c1e78a82bULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x041bb00df2e9eeb3ULL);
      }
   }
   else
   {
      printf("pclmulqdq_45 ... failed\n");
   }

   return;
}

static void pclmulqdq_46(void)
{
   reg128_t arg1 = { .uq = { 0x48abba8b80a64170ULL, 0x2455dd469f00dfa7ULL } };
   reg128_t arg2 = { .uq = { 0xd22aeea4262e2ec0ULL, 0x69157752f1c4d64fULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $1, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x4570a9283f65b1d0ULL && result0.uq[1] == 0x1937917bc5469bf6ULL )
      {
         printf("pclmulqdq_46 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_46 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x4570a9283f65b1d0ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x1937917bc5469bf6ULL);
      }
   }
   else
   {
      printf("pclmulqdq_46 ... failed\n");
   }

   return;
}

static void pclmulqdq_47(void)
{
   reg128_t arg1 = { .uq = { 0xf48abbaa4f902a14ULL, 0x7a455dd60675d3f9ULL } };
   reg128_t arg2 = { .uq = { 0xfd22aeebe9e8a8edULL, 0xbe915776dba21363ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $16, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x9708dd208b1f1635ULL && result0.uq[1] == 0x2911f19625f63a33ULL )
      {
         printf("pclmulqdq_47 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_47 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x9708dd208b1f1635ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x2911f19625f63a33ULL);
      }
   }
   else
   {
      printf("pclmulqdq_47 ... failed\n");
   }

   return;
}

static void pclmulqdq_48(void)
{
   reg128_t arg1 = { .uq = { 0x9f48abbc447ec8a2ULL, 0x4fa455df00ed2340ULL } };
   reg128_t arg2 = { .uq = { 0x27d22af05f24508fULL, 0xd3e91579063fe734ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $17, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x9e438129c0f21100ULL && result0.uq[1] == 0x302e6e9fad692a63ULL )
      {
         printf("pclmulqdq_48 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_48 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x9e438129c0f21100ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x302e6e9fad692a63ULL);
      }
   }
   else
   {
      printf("pclmulqdq_48 ... failed\n");
   }

   return;
}

static void pclmulqdq_49(void)
{
   reg128_t arg1 = { .uq = { 0x69f48abd61cdb289ULL, 0xf4fa455f97949835ULL } };
   reg128_t arg2 = { .uq = { 0xba7d22b0a2780b07ULL, 0x9d3e915937e9c470ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $0, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x77453ab2e39bcebfULL && result0.uq[1] == 0x3cd48149e75425dcULL )
      {
         printf("pclmulqdq_49 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_49 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x77453ab2e39bcebfULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x3cd48149e75425dcULL);
      }
   }
   else
   {
      printf("pclmulqdq_49 ... failed\n");
   }

   return;
}

static void pclmulqdq_50(void)
{
   reg128_t arg1 = { .uq = { 0x4e9f48ad7aa2a127ULL, 0xe74fa45793ff0f80ULL } };
   reg128_t arg2 = { .uq = { 0x73a7d22ca8ad46afULL, 0xf9d3e9173b046244ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $1, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x6e590e31be91a35cULL && result0.uq[1] == 0x3bd8e3c886ba7063ULL )
      {
         printf("pclmulqdq_50 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_50 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x6e590e31be91a35cULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x3bd8e3c886ba7063ULL);
      }
   }
   else
   {
      printf("pclmulqdq_50 ... failed\n");
   }

   return;
}

static void pclmulqdq_51(void)
{
   reg128_t arg1 = { .uq = { 0x7ce9f48c7c2ff011ULL, 0xfe74fa4714c5b6f9ULL } };
   reg128_t arg2 = { .uq = { 0xbf3a7d2461109a6dULL, 0x9f9d3e9317360c23ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $16, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0xd73226ad987c91b5ULL && result0.uq[1] == 0x6a0d4938c709c850ULL )
      {
         printf("pclmulqdq_51 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_51 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0xd73226ad987c91b5ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x6a0d4938c709c850ULL);
      }
   }
   else
   {
      printf("pclmulqdq_51 ... failed\n");
   }

   return;
}

static void pclmulqdq_52(void)
{
   reg128_t arg1 = { .uq = { 0x8fce9f4a6248c502ULL, 0x47e74fa60fd22170ULL } };
   reg128_t arg2 = { .uq = { 0x23f3a7d3e696cfa7ULL, 0xd1f9d3ead9f926c0ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $17, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x098aae5ab281c400ULL && result0.uq[1] == 0x360f213f1f15053eULL )
      {
         printf("pclmulqdq_52 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_52 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x098aae5ab281c400ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x360f213f1f15053eULL);
      }
   }
   else
   {
      printf("pclmulqdq_52 ... failed\n");
   }

   return;
}

static void pclmulqdq_53(void)
{
   reg128_t arg1 = { .uq = { 0x68fce9f64baa524fULL, 0xf47e74fc0c82e814ULL } };
   reg128_t arg2 = { .uq = { 0x7a3f3a7ee4ef32f9ULL, 0xfd1f9d405925586dULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $0, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0xc7150c4a11569767ULL && result0.uq[1] == 0x1230b217e7562125ULL )
      {
         printf("pclmulqdq_53 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_53 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0xc7150c4a11569767ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x1230b217e7562125ULL);
      }
   }
   else
   {
      printf("pclmulqdq_53 ... failed\n");
   }

   return;
}

static void pclmulqdq_54(void)
{
   reg128_t arg1 = { .uq = { 0xbe8fcea103406b23ULL, 0x9f47e751684df482ULL } };
   reg128_t arg2 = { .uq = { 0x4fa3f3a992d4b930ULL, 0x27d1f9d5a8181b87ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $1, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x7ec8d0bb7e8acd69ULL && result0.uq[1] == 0x14938d347e59462dULL )
      {
         printf("pclmulqdq_54 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_54 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x7ec8d0bb7e8acd69ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x14938d347e59462dULL);
      }
   }
   else
   {
      printf("pclmulqdq_54 ... failed\n");
   }

   return;
}

static void pclmulqdq_55(void)
{
   reg128_t arg1 = { .uq = { 0xd3e8fcebbab9ccb0ULL, 0x69f47e76bc0aa547ULL } };
   reg128_t arg2 = { .uq = { 0xf4fa3f3c34b31190ULL, 0x7a7d1f9ef90747b7ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $16, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x4d62ab2759d0c0f0ULL && result0.uq[1] == 0x24a78a2ff2816467ULL )
      {
         printf("pclmulqdq_55 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_55 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x4d62ab2759d0c0f0ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x24a78a2ff2816467ULL);
      }
   }
   else
   {
      printf("pclmulqdq_55 ... failed\n");
   }

   return;
}

static void pclmulqdq_56(void)
{
   reg128_t arg1 = { .uq = { 0xfd3e8fd0533162c8ULL, 0x7e9f47e908467053ULL } };
   reg128_t arg2 = { .uq = { 0xff4fa3f56ad0f71aULL, 0x7fa7d1fb94163a7cULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $17, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0xb7fc82a330a83644ULL && result0.uq[1] == 0x152129527f84cd53ULL )
      {
         printf("pclmulqdq_56 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_56 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0xb7fc82a330a83644ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x152129527f84cd53ULL);
      }
   }
   else
   {
      printf("pclmulqdq_56 ... failed\n");
   }

   return;
}

static void pclmulqdq_57(void)
{
   reg128_t arg1 = { .uq = { 0x3fd3e8fea8b8dc2dULL, 0xdfe9f4803b0a2d03ULL } };
   reg128_t arg2 = { .uq = { 0xaff4fa40f432d572ULL, 0x57fa7d2158c729a8ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $0, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0xad24061697897d6aULL && result0.uq[1] == 0x1946dd2b6b334aa6ULL )
      {
         printf("pclmulqdq_57 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_57 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0xad24061697897d6aULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x1946dd2b6b334aa6ULL);
      }
   }
   else
   {
      printf("pclmulqdq_57 ... failed\n");
   }

   return;
}

static void pclmulqdq_58(void)
{
   reg128_t arg1 = { .uq = { 0x2bfd3e918b1153c3ULL, 0xd5fe9f49ac3668d2ULL } };
   reg128_t arg2 = { .uq = { 0x6aff4fa5b4c8f358ULL, 0x357fa7d3b912389bULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $1, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x01e66902d969ffedULL && result0.uq[1] == 0x0748de913628c280ULL )
      {
         printf("pclmulqdq_58 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_58 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x01e66902d969ffedULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x0748de913628c280ULL);
      }
   }
   else
   {
      printf("pclmulqdq_58 ... failed\n");
   }

   return;
}

static void pclmulqdq_59(void)
{
   reg128_t arg1 = { .uq = { 0xdabfd3eab336db3eULL, 0x6d5fe9f638492c8eULL } };
   reg128_t arg2 = { .uq = { 0x36aff4fbfad25536ULL, 0x1b57fa7edc16e98aULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $16, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x9a16d14091086404ULL && result0.uq[1] == 0x0a2888aa8b3b5dd4ULL )
      {
         printf("pclmulqdq_59 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_59 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x9a16d14091086404ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x0a2888aa8b3b5dd4ULL);
      }
   }
   else
   {
      printf("pclmulqdq_59 ... failed\n");
   }

   return;
}

static void pclmulqdq_60(void)
{
   reg128_t arg1 = { .uq = { 0x0dabfd404cb933b4ULL, 0x06d5fea1050a58c9ULL } };
   reg128_t arg2 = { .uq = { 0xc36aff516932eb55ULL, 0xa1b57fa99b473497ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $17, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x3f904f57b277f66fULL && result0.uq[1] == 0x03b554c0f32d4dfaULL )
      {
         printf("pclmulqdq_60 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_60 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x3f904f57b277f66fULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x03b554c0f32d4dfaULL);
      }
   }
   else
   {
      printf("pclmulqdq_60 ... failed\n");
   }

   return;
}

static void pclmulqdq_61(void)
{
   reg128_t arg1 = { .uq = { 0x90dabfd5a4515938ULL, 0x486d5febb0d66b8bULL } };
   reg128_t arg2 = { .uq = { 0xe436aff6af18f4b6ULL, 0x721b57fc363a394aULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $0, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x8c5f6ad18d379e10ULL && result0.uq[1] == 0x7c1be44f7439dfecULL )
      {
         printf("pclmulqdq_61 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_61 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x8c5f6ad18d379e10ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x7c1be44f7439dfecULL);
      }
   }
   else
   {
      printf("pclmulqdq_61 ... failed\n");
   }

   return;
}

static void pclmulqdq_62(void)
{
   reg128_t arg1 = { .uq = { 0x390dabfef9cadb94ULL, 0x1c86d6005b932cb9ULL } };
   reg128_t arg2 = { .uq = { 0xce436b010477554dULL, 0xa721b58168e96993ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $1, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x5cfbe32f78bbabfcULL && result0.uq[1] == 0x1b0f409db94d30ceULL )
      {
         printf("pclmulqdq_62 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_62 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x5cfbe32f78bbabfcULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x1b0f409db94d30ceULL);
      }
   }
   else
   {
      printf("pclmulqdq_62 ... failed\n");
   }

   return;
}

static void pclmulqdq_63(void)
{
   reg128_t arg1 = { .uq = { 0x9390dac19b2273baULL, 0x49c86d61ac3ef8ccULL } };
   reg128_t arg2 = { .uq = { 0x24e436b1b4cd3b55ULL, 0xd2721b59b1145c97ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $16, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x6a2bef2bac52d03cULL && result0.uq[1] == 0x0820a820580aebf6ULL )
      {
         printf("pclmulqdq_63 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_63 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x6a2bef2bac52d03cULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x0820a820580aebf6ULL);
      }
   }
   else
   {
      printf("pclmulqdq_63 ... failed\n");
   }

   return;
}

static void pclmulqdq_64(void)
{
   reg128_t arg1 = { .uq = { 0xa9390dadaf37ed38ULL, 0x549c86d7b649b58bULL } };
   reg128_t arg2 = { .uq = { 0xea4e436cb1d299b6ULL, 0x752721b737970bcaULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $17, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0xe374fdf10b9aa50eULL && result0.uq[1] == 0x1bf0a13b9a4b2d32ULL )
      {
         printf("pclmulqdq_64 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_64 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0xe374fdf10b9aa50eULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x1bf0a13b9a4b2d32ULL);
      }
   }
   else
   {
      printf("pclmulqdq_64 ... failed\n");
   }

   return;
}

static void pclmulqdq_65(void)
{
   reg128_t arg1 = { .uq = { 0x3a9390dc7a7944d4ULL, 0x1d49c86f1bea6159ULL } };
   reg128_t arg2 = { .uq = { 0xcea4e43864a2ef9dULL, 0xa752721d18ff36bbULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $0, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0xd6c456490d755a64ULL && result0.uq[1] == 0x12bc3c19097f5a00ULL )
      {
         printf("pclmulqdq_65 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_65 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0xd6c456490d755a64ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x12bc3c19097f5a00ULL);
      }
   }
   else
   {
      printf("pclmulqdq_65 ... failed\n");
   }

   return;
}

static void pclmulqdq_66(void)
{
   reg128_t arg1 = { .uq = { 0x93a9390f632d5a4eULL, 0x49d49c8890446c16ULL } };
   reg128_t arg2 = { .uq = { 0x24ea4e4526cff4faULL, 0x127527237215b96cULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $1, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x8d19b35c04f67f08ULL && result0.uq[1] == 0x0820a88940df4faaULL )
      {
         printf("pclmulqdq_66 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_66 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x8d19b35c04f67f08ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x0820a88940df4faaULL);
      }
   }
   else
   {
      printf("pclmulqdq_66 ... failed\n");
   }

   return;
}

static void pclmulqdq_67(void)
{
   reg128_t arg1 = { .uq = { 0x093a939297b89ba5ULL, 0xc49d49ca228a0cbfULL } };
   reg128_t arg2 = { .uq = { 0xa24ea4e5f7f2c54cULL, 0x51275273daa72195ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $16, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0xa98a2c26439b7bc4ULL && result0.uq[1] == 0x7b61d63f58d2a46aULL )
      {
         printf("pclmulqdq_67 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_67 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0xa98a2c26439b7bc4ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x7b61d63f58d2a46aULL);
      }
   }
   else
   {
      printf("pclmulqdq_67 ... failed\n");
   }

   return;
}

static void pclmulqdq_68(void)
{
   reg128_t arg1 = { .uq = { 0xe893a93ac4014fb7ULL, 0xb449d49e48ae66c8ULL } };
   reg128_t arg2 = { .uq = { 0x5a24ea500304f253ULL, 0xed127528e830381aULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $17, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0xcd6a96a3f2ca5750ULL && result0.uq[1] == 0x66729b7e79914803ULL )
      {
         printf("pclmulqdq_68 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_68 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0xcd6a96a3f2ca5750ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x66729b7e79914803ULL);
      }
   }
   else
   {
      printf("pclmulqdq_68 ... failed\n");
   }

   return;
}

static void pclmulqdq_69(void)
{
   reg128_t arg1 = { .uq = { 0x76893a9552c5dafcULL, 0x3b449d4b8810ac6dULL } };
   reg128_t arg2 = { .uq = { 0xdda24ea6aab61523ULL, 0xaed127543c08c982ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $0, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x71e1947ce8a3fc84ULL && result0.uq[1] == 0x23a3c3ff0ac48e0cULL )
      {
         printf("pclmulqdq_69 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_69 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x71e1947ce8a3fc84ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x23a3c3ff0ac48e0cULL);
      }
   }
   else
   {
      printf("pclmulqdq_69 ... failed\n");
   }

   return;
}

static void pclmulqdq_70(void)
{
   reg128_t arg1 = { .uq = { 0x576893aafcb223b0ULL, 0x2bb449d65d06d0c7ULL } };
   reg128_t arg2 = { .uq = { 0xd5da24ec05312750ULL, 0x6aed1276e1465297ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $1, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x83bc107e3e1d6910ULL && result0.uq[1] == 0x1d159417367f29d6ULL )
      {
         printf("pclmulqdq_70 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_70 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x83bc107e3e1d6910ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x1d159417367f29d6ULL);
      }
   }
   else
   {
      printf("pclmulqdq_70 ... failed\n");
   }

   return;
}

static void pclmulqdq_71(void)
{
   reg128_t arg1 = { .uq = { 0xf576893c5750e838ULL, 0x7abb449f0a56330bULL } };
   reg128_t arg2 = { .uq = { 0xfd5da2506bd8d876ULL, 0x7eaed129149a2b2aULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $16, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x162d65810a9a912aULL && result0.uq[1] == 0x295151cffabdfebcULL )
      {
         printf("pclmulqdq_71 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_71 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x162d65810a9a912aULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x295151cffabdfebcULL);
      }
   }
   else
   {
      printf("pclmulqdq_71 ... failed\n");
   }

   return;
}

static void pclmulqdq_72(void)
{
   reg128_t arg1 = { .uq = { 0x3f57689568fad484ULL, 0x1fabb44b932b2931ULL } };
   reg128_t arg2 = { .uq = { 0xcfd5da26a0435389ULL, 0xa7eaed1436cf68b5ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $17, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0xd2b7ff103f384845ULL && result0.uq[1] == 0x0c75fdbda2b7fa1dULL )
      {
         printf("pclmulqdq_72 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_72 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0xd2b7ff103f384845ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x0c75fdbda2b7fa1dULL);
      }
   }
   else
   {
      printf("pclmulqdq_72 ... failed\n");
   }

   return;
}

static void pclmulqdq_73(void)
{
   reg128_t arg1 = { .uq = { 0x93f5768af2157347ULL, 0x89fabb464fb87890ULL } };
   reg128_t arg2 = { .uq = { 0x44fd5da40689fb37ULL, 0xe27eaed2e9f2bc88ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $0, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x1efac67fed4d2545ULL && result0.uq[1] == 0x26c2a63498b85e4eULL )
      {
         printf("pclmulqdq_73 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_73 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x1efac67fed4d2545ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x26c2a63498b85e4eULL);
      }
   }
   else
   {
      printf("pclmulqdq_73 ... failed\n");
   }

   return;
}

static void pclmulqdq_74(void)
{
   reg128_t arg1 = { .uq = { 0x713f576a53a71d33ULL, 0xf89fabb600814d8aULL } };
   reg128_t arg2 = { .uq = { 0x7c4fd5dbdeee65b4ULL, 0x3e27eaeece24f1c9ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $1, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x25ac28bd28c702ebULL && result0.uq[1] == 0x0b943f31e76dc46aULL )
      {
         printf("pclmulqdq_74 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_74 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x25ac28bd28c702ebULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x0b943f31e76dc46aULL);
      }
   }
   else
   {
      printf("pclmulqdq_74 ... failed\n");
   }

   return;
}

static void pclmulqdq_75(void)
{
   reg128_t arg1 = { .uq = { 0xdf13f5784dc037d5ULL, 0xaf89fabd0d8ddad7ULL } };
   reg128_t arg2 = { .uq = { 0x97c4fd5f6d74ac58ULL, 0x4be27eb09568151bULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $16, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x88ede6d4e2454a08ULL && result0.uq[1] == 0x5e0c23d1ad9c93aaULL )
      {
         printf("pclmulqdq_75 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_75 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x88ede6d4e2454a08ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x5e0c23d1ad9c93aaULL);
      }
   }
   else
   {
      printf("pclmulqdq_75 ... failed\n");
   }

   return;
}

static void pclmulqdq_76(void)
{
   reg128_t arg1 = { .uq = { 0xe5f13f592161c97eULL, 0x72f89fad6f5ea3aeULL } };
   reg128_t arg2 = { .uq = { 0x397c4fd7965d10c6ULL, 0x1cbe27eca9dc4752ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $17, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x5dd6030446d65c3cULL && result0.uq[1] == 0x0541155160eaa5f3ULL )
      {
         printf("pclmulqdq_76 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_76 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x5dd6030446d65c3cULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x0541155160eaa5f3ULL);
      }
   }
   else
   {
      printf("pclmulqdq_76 ... failed\n");
   }

   return;
}

static void pclmulqdq_77(void)
{
   reg128_t arg1 = { .uq = { 0x0e5f13f7339be298ULL, 0x072f89fc787bb03bULL } };
   reg128_t arg2 = { .uq = { 0xc397c4ff12eb970eULL, 0x61cbe28068238a76ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $0, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x9d37888783271390ULL && result0.uq[1] == 0x04ad49530ddeba57ULL )
      {
         printf("pclmulqdq_77 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_77 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x9d37888783271390ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x04ad49530ddeba57ULL);
      }
   }
   else
   {
      printf("pclmulqdq_77 ... failed\n");
   }

   return;
}

static void pclmulqdq_78(void)
{
   reg128_t arg1 = { .uq = { 0x30e5f14112bf842aULL, 0x1872f8a1680d8104ULL } };
   reg128_t arg2 = { .uq = { 0x0c397c5192b47f71ULL, 0xc61cbe29a007fea9ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $1, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0xda22a0000546f93aULL && result0.uq[1] == 0x14eb8e72cfa9f51dULL )
      {
         printf("pclmulqdq_78 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_78 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0xda22a0000546f93aULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x14eb8e72cfa9f51dULL);
      }
   }
   else
   {
      printf("pclmulqdq_78 ... failed\n");
   }

   return;
}

static void pclmulqdq_79(void)
{
   reg128_t arg1 = { .uq = { 0xa30e5f15b6b1be45ULL, 0x91872f8bb2069e0fULL } };
   reg128_t arg2 = { .uq = { 0x88c397c6afb10df4ULL, 0x4461cbe4368645e9ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $16, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x0afb9d418fc1966cULL && result0.uq[1] == 0x4c22fc2c395f9b0aULL )
      {
         printf("pclmulqdq_79 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_79 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x0afb9d418fc1966cULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x4c22fc2c395f9b0aULL);
      }
   }
   else
   {
      printf("pclmulqdq_79 ... failed\n");
   }

   return;
}

static void pclmulqdq_80(void)
{
   reg128_t arg1 = { .uq = { 0xe230e5f2f1f0e1e5ULL, 0xb11872fa4fa62fdfULL } };
   reg128_t arg2 = { .uq = { 0x988c397e0e80d6dcULL, 0x4c461cbfe5ee2a5dULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $17, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x4a4c895ad9a6426bULL && result0.uq[1] == 0x2b2552b15c95b45aULL )
      {
         printf("pclmulqdq_80 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_80 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x4a4c895ad9a6426bULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x2b2552b15c95b45aULL);
      }
   }
   else
   {
      printf("pclmulqdq_80 ... failed\n");
   }

   return;
}

static void pclmulqdq_81(void)
{
   reg128_t arg1 = { .uq = { 0xe6230e60d9a4d41bULL, 0xb3118731438028feULL } };
   reg128_t arg2 = { .uq = { 0x5988c399806dd36eULL, 0x2cc461cd9ee4a8a6ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $0, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0xb4bce4629b6a0022ULL && result0.uq[1] == 0x3049a355e88b747dULL )
      {
         printf("pclmulqdq_81 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_81 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0xb4bce4629b6a0022ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x3049a355e88b747dULL);
      }
   }
   else
   {
      printf("pclmulqdq_81 ... failed\n");
   }

   return;
}

static void pclmulqdq_82(void)
{
   reg128_t arg1 = { .uq = { 0x166230e7ae201342ULL, 0x0b311874b5bdc890ULL } };
   reg128_t arg2 = { .uq = { 0x05988c3b398ca337ULL, 0xc2cc461e73741088ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $1, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x1443368a87d51b10ULL && result0.uq[1] == 0x0e8b16ca9bb0a8d6ULL )
      {
         printf("pclmulqdq_82 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_82 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x1443368a87d51b10ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x0e8b16ca9bb0a8d6ULL);
      }
   }
   else
   {
      printf("pclmulqdq_82 ... failed\n");
   }

   return;
}

static void pclmulqdq_83(void)
{
   reg128_t arg1 = { .uq = { 0x616623101867c733ULL, 0xf0b31188e2e1a28aULL } };
   reg128_t arg2 = { .uq = { 0x785988c5501e9034ULL, 0x3c2cc46386bd0709ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $16, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x505b0a15e10953c8ULL && result0.uq[1] == 0x2a8022826ef2c0edULL )
      {
         printf("pclmulqdq_83 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_83 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x505b0a15e10953c8ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x2a8022826ef2c0edULL);
      }
   }
   else
   {
      printf("pclmulqdq_83 ... failed\n");
   }

   return;
}

static void pclmulqdq_84(void)
{
   reg128_t arg1 = { .uq = { 0xde166232aa0c4275ULL, 0xaf0b311a3bb3e027ULL } };
   reg128_t arg2 = { .uq = { 0x9785988df487af00ULL, 0x4bc2cc47d8f1966fULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $17, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x73b0bf7077a68eedULL && result0.uq[1] == 0x2f36ea74050c13feULL )
      {
         printf("pclmulqdq_84 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_84 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x73b0bf7077a68eedULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x2f36ea74050c13feULL);
      }
   }
   else
   {
      printf("pclmulqdq_84 ... failed\n");
   }

   return;
}

static void pclmulqdq_85(void)
{
   reg128_t arg1 = { .uq = { 0xe5e16624c3268a24ULL, 0x72f0b31340410401ULL } };
   reg128_t arg2 = { .uq = { 0xf978598a86ce40f1ULL, 0xbcbc2cc62a14df69ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $0, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0xbd20232a4309f7e4ULL && result0.uq[1] == 0x5e8cbf9a58306d67ULL )
      {
         printf("pclmulqdq_85 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_85 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0xbd20232a4309f7e4ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x5e8cbf9a58306d67ULL);
      }
   }
   else
   {
      printf("pclmulqdq_85 ... failed\n");
   }

   return;
}

static void pclmulqdq_86(void)
{
   reg128_t arg1 = { .uq = { 0x9e5e1663fbb82ea5ULL, 0x8f2f0b32d489d63fULL } };
   reg128_t arg2 = { .uq = { 0x8797859a40f2aa0cULL, 0x43cbc2cdff2713f5ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $1, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x6e66a095df81ee01ULL && result0.uq[1] == 0x2658e6d490286e46ULL )
      {
         printf("pclmulqdq_86 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_86 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x6e66a095df81ee01ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x2658e6d490286e46ULL);
      }
   }
   else
   {
      printf("pclmulqdq_86 ... failed\n");
   }

   return;
}

static void pclmulqdq_87(void)
{
   reg128_t arg1 = { .uq = { 0xe1e5e167d64148e7ULL, 0xb0f2f0b4c1ce6360ULL } };
   reg128_t arg2 = { .uq = { 0x5879785b3f94f09fULL, 0xec3cbc2e7678373cULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $16, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0xa1411d12187db520ULL && result0.uq[1] == 0x22802a82bbe48273ULL )
      {
         printf("pclmulqdq_87 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_87 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0xa1411d12187db520ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x22802a82bbe48273ULL);
      }
   }
   else
   {
      printf("pclmulqdq_87 ... failed\n");
   }

   return;
}

static void pclmulqdq_88(void)
{
   reg128_t arg1 = { .uq = { 0x761e5e1819e9da8dULL, 0xfb0f2f0ce3a2ac33ULL } };
   reg128_t arg2 = { .uq = { 0xbd879787587f150aULL, 0x5ec3cbc48aed4974ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $17, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x98102e76a4d8925cULL && result0.uq[1] == 0x34f36e356807a7b8ULL )
      {
         printf("pclmulqdq_88 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_88 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x98102e76a4d8925cULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x34f36e356807a7b8ULL);
      }
   }
   else
   {
      printf("pclmulqdq_88 ... failed\n");
   }

   return;
}

static void pclmulqdq_89(void)
{
   reg128_t arg1 = { .uq = { 0x2f61e5e3242463a9ULL, 0xd7b0f2f278bff0c5ULL } };
   reg128_t arg2 = { .uq = { 0xabd8797a130db74fULL, 0x95ec3cbde0349a94ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $0, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0xa9db52b34932b257ULL && result0.uq[1] == 0x13498b4de2ead42bULL )
      {
         printf("pclmulqdq_89 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_89 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0xa9db52b34932b257ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x13498b4de2ead42bULL);
      }
   }
   else
   {
      printf("pclmulqdq_89 ... failed\n");
   }

   return;
}

static void pclmulqdq_90(void)
{
   reg128_t arg1 = { .uq = { 0x4af61e5fcec80c39ULL, 0xe57b0f30ce11c50dULL } };
   reg128_t arg2 = { .uq = { 0xb2bd87994db6a173ULL, 0x995ec3cd8d890faaULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $1, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x02bdfd5b8ae3851aULL && result0.uq[1] == 0x23dce4693be7adf0ULL )
      {
         printf("pclmulqdq_90 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_90 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x02bdfd5b8ae3851aULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x23dce4693be7adf0ULL);
      }
   }
   else
   {
      printf("pclmulqdq_90 ... failed\n");
   }

   return;
}

static void pclmulqdq_91(void)
{
   reg128_t arg1 = { .uq = { 0x4caf61e7a57246c4ULL, 0x2657b0f4b166e251ULL } };
   reg128_t arg2 = { .uq = { 0xd32bd87b2f613019ULL, 0xa995ec3e7e5e56fdULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $16, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x65396ddebd61e5c9ULL && result0.uq[1] == 0x18b43ccdffec0aabULL )
      {
         printf("pclmulqdq_91 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_91 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x65396ddebd61e5c9ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x18b43ccdffec0aabULL);
      }
   }
   else
   {
      printf("pclmulqdq_91 ... failed\n");
   }

   return;
}

static void pclmulqdq_92(void)
{
   reg128_t arg1 = { .uq = { 0x94caf62015dcea6bULL, 0x8a657b10e19c3426ULL } };
   reg128_t arg2 = { .uq = { 0x4532bd894f7bd902ULL, 0x22995ec5866bab70ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $17, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0xe5df1fa230385520ULL && result0.uq[1] == 0x101105049484270bULL )
      {
         printf("pclmulqdq_92 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_92 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0xe5df1fa230385520ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x101105049484270bULL);
      }
   }
   else
   {
      printf("pclmulqdq_92 ... failed\n");
   }

   return;
}

static void pclmulqdq_93(void)
{
   reg128_t arg1 = { .uq = { 0x114caf63a1e394a7ULL, 0xc8a657b2b79f8940ULL } };
   reg128_t arg2 = { .uq = { 0x64532bda3a7d838fULL, 0xf22995edf3ec80b4ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $0, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x14b59c3f07c170cdULL && result0.uq[1] == 0x063afa20a20bf1d3ULL )
      {
         printf("pclmulqdq_93 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_93 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x14b59c3f07c170cdULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x063afa20a20bf1d3ULL);
      }
   }
   else
   {
      printf("pclmulqdq_93 ... failed\n");
   }

   return;
}

static void pclmulqdq_94(void)
{
   reg128_t arg1 = { .uq = { 0x7914caf7d8a3ff49ULL, 0xfc8a657cc2ffbe95ULL } };
   reg128_t arg2 = { .uq = { 0xbe4532bf482d9e37ULL, 0x9f2299608ac48e08ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $1, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0xd1b3a3b63ca68448ULL && result0.uq[1] == 0x39b309fce070d54fULL )
      {
         printf("pclmulqdq_94 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_94 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0xd1b3a3b63ca68448ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x39b309fce070d54fULL);
      }
   }
   else
   {
      printf("pclmulqdq_94 ... failed\n");
   }

   return;
}

static void pclmulqdq_95(void)
{
   reg128_t arg1 = { .uq = { 0x4f914cb1241005f3ULL, 0xe7c8a65978b5c1eaULL } };
   reg128_t arg2 = { .uq = { 0x73e4532d9b089fe4ULL, 0x39f22997ac320ee1ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $16, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0xfe28658d590ed368ULL && result0.uq[1] == 0x2a0aa820dbbaae83ULL )
      {
         printf("pclmulqdq_95 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_95 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0xfe28658d590ed368ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x2a0aa820dbbaae83ULL);
      }
   }
   else
   {
      printf("pclmulqdq_95 ... failed\n");
   }

   return;
}

static void pclmulqdq_96(void)
{
   reg128_t arg1 = { .uq = { 0xdcf914ccbcc6c661ULL, 0xae7c8a6735112221ULL } };
   reg128_t arg2 = { .uq = { 0x973e453471365001ULL, 0x8b9f229b0f48e6f1ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $17, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x12081cb0e7a6fad1ULL && result0.uq[1] == 0x53e4d12a4d38b461ULL )
      {
         printf("pclmulqdq_96 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_96 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x12081cb0e7a6fad1ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x53e4d12a4d38b461ULL);
      }
   }
   else
   {
      printf("pclmulqdq_96 ... failed\n");
   }

   return;
}

static void pclmulqdq_97(void)
{
   reg128_t arg1 = { .uq = { 0x85cf914e6e523269ULL, 0x82e7c8a81dd6d825ULL } };
   reg128_t arg2 = { .uq = { 0x8173e454e5992affULL, 0x80b9f22b597a546cULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $0, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x747ebfee06547327ULL && result0.uq[1] == 0x425a6e980d6f32f8ULL )
      {
         printf("pclmulqdq_97 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_97 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x747ebfee06547327ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x425a6e980d6f32f8ULL);
      }
   }
   else
   {
      printf("pclmulqdq_97 ... failed\n");
   }

   return;
}

static void pclmulqdq_98(void)
{
   reg128_t arg1 = { .uq = { 0x405cf9168b6ae925ULL, 0xe02e7c8c2c63337fULL } };
   reg128_t arg2 = { .uq = { 0xb0173e46fcdf58acULL, 0x580b9f245d1d6b45ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $1, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x10d3ee4c6cada3f1ULL && result0.uq[1] == 0x1612f3ff623fded0ULL )
      {
         printf("pclmulqdq_98 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_98 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x10d3ee4c6cada3f1ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x1612f3ff623fded0ULL);
      }
   }
   else
   {
      printf("pclmulqdq_98 ... failed\n");
   }

   return;
}

static void pclmulqdq_99(void)
{
   reg128_t arg1 = { .uq = { 0xec05cf93053c748fULL, 0xb602e7ca694bf934ULL } };
   reg128_t arg2 = { .uq = { 0x5b0173e61353bb89ULL, 0xed80b9f3e0579cb5ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $16, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x465f8b79f0199694ULL && result0.uq[1] == 0x228a0003e5a76b0cULL )
      {
         printf("pclmulqdq_99 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_99 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x465f8b79f0199694ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x228a0003e5a76b0cULL);
      }
   }
   else
   {
      printf("pclmulqdq_99 ... failed\n");
   }

   return;
}

static void pclmulqdq_100(void)
{
   reg128_t arg1 = { .uq = { 0xb6c05cfad6d98d47ULL, 0x9b602e7e421a8590ULL } };
   reg128_t arg2 = { .uq = { 0x4db0173fffbb01b7ULL, 0xe6d80ba0d68b3fc8ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $17, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0xf7caf3aef858f080ULL && result0.uq[1] == 0x7b3959bd543d5319ULL )
      {
         printf("pclmulqdq_100 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_100 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0xf7caf3aef858f080ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x7b3959bd543d5319ULL);
      }
   }
   else
   {
      printf("pclmulqdq_100 ... failed\n");
   }

   return;
}

static void pclmulqdq_101(void)
{
   reg128_t arg1 = { .uq = { 0x736c05d149f35ed3ULL, 0xf9b602e98ba76e5aULL } };
   reg128_t arg2 = { .uq = { 0x7cdb0175a481761cULL, 0x3e6d80bbb0ee79fdULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $0, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x4d7e2029ce741ae4ULL && result0.uq[1] == 0x17f7c4da2519e2fbULL )
      {
         printf("pclmulqdq_101 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_101 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x4d7e2029ce741ae4ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x17f7c4da2519e2fbULL);
      }
   }
   else
   {
      printf("pclmulqdq_101 ... failed\n");
   }

   return;
}

static void pclmulqdq_102(void)
{
   reg128_t arg1 = { .uq = { 0xdf36c05eaf24fbebULL, 0xaf9b60303e403ce6ULL } };
   reg128_t arg2 = { .uq = { 0x57cdb018fdcddd62ULL, 0x2be6d80d5d94ada0ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $1, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x0b6d29afcf3d77e0ULL && result0.uq[1] == 0x1c3f14ae1ee34afcULL )
      {
         printf("pclmulqdq_102 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_102 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x0b6d29afcf3d77e0ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x1c3f14ae1ee34afcULL);
      }
   }
   else
   {
      printf("pclmulqdq_102 ... failed\n");
   }

   return;
}

static void pclmulqdq_103(void)
{
   reg128_t arg1 = { .uq = { 0x15f36c078d7815bfULL, 0xcaf9b604ad69c9ccULL } };
   reg128_t arg2 = { .uq = { 0x657cdb033562a3d5ULL, 0xf2be6d82715f10d7ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $16, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x45492b482259333cULL && result0.uq[1] == 0x28222aa042940471ULL )
      {
         printf("pclmulqdq_103 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_103 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x45492b482259333cULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x28222aa042940471ULL);
      }
   }
   else
   {
      printf("pclmulqdq_103 ... failed\n");
   }

   return;
}

static void pclmulqdq_104(void)
{
   reg128_t arg1 = { .uq = { 0xb95f36c20f5d4758ULL, 0x5caf9b61e65c629bULL } };
   reg128_t arg2 = { .uq = { 0xee57cdb1d9dbf03eULL, 0x772be6d9cb9bb70eULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $17, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0xa564c7636b75ca82ULL && result0.uq[1] == 0x18ea04a16910639dULL )
      {
         printf("pclmulqdq_104 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_104 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0xa564c7636b75ca82ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x18ea04a16910639dULL);
      }
   }
   else
   {
      printf("pclmulqdq_104 ... failed\n");
   }

   return;
}

static void pclmulqdq_105(void)
{
   reg128_t arg1 = { .uq = { 0x3b95f36dc47b9a76ULL, 0x1dcaf9b7c0eb8c2aULL } };
   reg128_t arg2 = { .uq = { 0x0ee57cdcbf238504ULL, 0x0772be6f3e3f8171ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $0, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x93da7ceefb7cc7d8ULL && result0.uq[1] == 0x01515044e5c70080ULL )
      {
         printf("pclmulqdq_105 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_105 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x93da7ceefb7cc7d8ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x01515044e5c70080ULL);
      }
   }
   else
   {
      printf("pclmulqdq_105 ... failed\n");
   }

   return;
}

static void pclmulqdq_106(void)
{
   reg128_t arg1 = { .uq = { 0xc3b95f3875cd7fa9ULL, 0xa1dcaf9d11947ec5ULL } };
   reg128_t arg2 = { .uq = { 0x90ee57cf5f77fe4fULL, 0x88772be88669be14ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $1, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x1ff2cadc63448a34ULL && result0.uq[1] == 0x67e792c3afe8cc55ULL )
      {
         printf("pclmulqdq_106 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_106 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x1ff2cadc63448a34ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x67e792c3afe8cc55ULL);
      }
   }
   else
   {
      printf("pclmulqdq_106 ... failed\n");
   }

   return;
}

static void pclmulqdq_107(void)
{
   reg128_t arg1 = { .uq = { 0x443b95f521e29df9ULL, 0xe21dcafb779f0dedULL } };
   reg128_t arg2 = { .uq = { 0xb10ee57e927d45e3ULL, 0x988772c01fec61e2ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $16, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x0fa35a87a4453f57ULL && result0.uq[1] == 0x638b976af3ff9af9ULL )
      {
         printf("pclmulqdq_107 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_107 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x0fa35a87a4453f57ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x638b976af3ff9af9ULL);
      }
   }
   else
   {
      printf("pclmulqdq_107 ... failed\n");
   }

   return;
}

static void pclmulqdq_108(void)
{
   reg128_t arg1 = { .uq = { 0x4c43b960eea3efe0ULL, 0x2621dcb155ffb6dfULL } };
   reg128_t arg2 = { .uq = { 0xd310ee5981ad9a5cULL, 0x6988772d9f848c1dULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $17, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x51c023c60bf9b2abULL && result0.uq[1] == 0x0c494dba164d6a1aULL )
      {
         printf("pclmulqdq_108 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_108 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x51c023c60bf9b2abULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x0c494dba164d6a1aULL);
      }
   }
   else
   {
      printf("pclmulqdq_108 ... failed\n");
   }

   return;
}

static void pclmulqdq_109(void)
{
   reg128_t arg1 = { .uq = { 0xf4c43b97a67004fbULL, 0xba621dccb9e5c16eULL } };
   reg128_t arg2 = { .uq = { 0x5d310ee73ba09fa6ULL, 0x2e9887747c7e0ec2ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $0, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x031c347016c9f1faULL && result0.uq[1] == 0x36ed9d92bc682f1eULL )
      {
         printf("pclmulqdq_109 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_109 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x031c347016c9f1faULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x36ed9d92bc682f1eULL);
      }
   }
   else
   {
      printf("pclmulqdq_109 ... failed\n");
   }

   return;
}

static void pclmulqdq_110(void)
{
   reg128_t arg1 = { .uq = { 0x174c43bb1cecc650ULL, 0x0ba621de6d242217ULL } };
   reg128_t arg2 = { .uq = { 0xc5d310f01d3fcff8ULL, 0x62e98878ed4da6ebULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $1, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0xacf5238d6a7eee70ULL && result0.uq[1] == 0x07183a9373413ee5ULL )
      {
         printf("pclmulqdq_110 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_110 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0xacf5238d6a7eee70ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x07183a9373413ee5ULL);
      }
   }
   else
   {
      printf("pclmulqdq_110 ... failed\n");
   }

   return;
}

static void pclmulqdq_111(void)
{
   reg128_t arg1 = { .uq = { 0xf174c43d5d549266ULL, 0x78ba621f8d580822ULL } };
   reg128_t arg2 = { .uq = { 0x3c5d3110a559c300ULL, 0x1e2e9889315aa06fULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $16, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0xf27d9c6ac793e600ULL && result0.uq[1] == 0x0aa022a73ecb539dULL )
      {
         printf("pclmulqdq_111 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_111 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0xf27d9c6ac793e600ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x0aa022a73ecb539dULL);
      }
   }
   else
   {
      printf("pclmulqdq_111 ... failed\n");
   }

   return;
}

static void pclmulqdq_112(void)
{
   reg128_t arg1 = { .uq = { 0xcf174c456f5b0f24ULL, 0x678ba623965b4681ULL } };
   reg128_t arg2 = { .uq = { 0xf3c5d312a1db6231ULL, 0xb9e2e98a379b7009ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $17, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0xd950b5c449820289ULL && result0.uq[1] == 0x3a31f7c40b66ebf6ULL )
      {
         printf("pclmulqdq_112 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_112 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0xd950b5c449820289ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x3a31f7c40b66ebf6ULL);
      }
   }
   else
   {
      printf("pclmulqdq_112 ... failed\n");
   }

   return;
}

static void pclmulqdq_113(void)
{
   reg128_t arg1 = { .uq = { 0x9cf174c5f27b76f5ULL, 0x8e78ba63cfeb7a67ULL } };
   reg128_t arg2 = { .uq = { 0x873c5d32cea37c20ULL, 0x439e2e9a45ff7cffULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $0, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0xd8c13a16143112a0ULL && result0.uq[1] == 0x4db281bbf229390cULL )
      {
         printf("pclmulqdq_113 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_113 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0xd8c13a16143112a0ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x4db281bbf229390cULL);
      }
   }
   else
   {
      printf("pclmulqdq_113 ... failed\n");
   }

   return;
}

static void pclmulqdq_114(void)
{
   reg128_t arg1 = { .uq = { 0xe1cf174e09ad7d6cULL, 0x70e78ba7e3847da5ULL } };
   reg128_t arg2 = { .uq = { 0xf873c5d4d86ffdbfULL, 0xbc39e2eb42e5bdccULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $1, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x71ad667673cd0fd0ULL && result0.uq[1] == 0x665e4345d7dcc057ULL )
      {
         printf("pclmulqdq_114 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_114 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x71ad667673cd0fd0ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x665e4345d7dcc057ULL);
      }
   }
   else
   {
      printf("pclmulqdq_114 ... failed\n");
   }

   return;
}

static void pclmulqdq_115(void)
{
   reg128_t arg1 = { .uq = { 0x5e1cf17680209dd5ULL, 0xef0e78bc26be0dd7ULL } };
   reg128_t arg2 = { .uq = { 0xb7873c5efa0cc5d8ULL, 0x5bc39e305bb421dbULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $16, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x4b74700bf2d5e688ULL && result0.uq[1] == 0x666e225b4656f8e1ULL )
      {
         printf("pclmulqdq_115 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_115 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x4b74700bf2d5e688ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x666e225b4656f8e1ULL);
      }
   }
   else
   {
      printf("pclmulqdq_115 ... failed\n");
   }

   return;
}

static void pclmulqdq_116(void)
{
   reg128_t arg1 = { .uq = { 0xede1cf190487cfdeULL, 0x76f0e78d60f1a6deULL } };
   reg128_t arg2 = { .uq = { 0x3b7873c78f26925eULL, 0x1dbc39e4a641081eULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $17, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x17a07657d2da7dd4ULL && result0.uq[1] == 0x0545154178351fcaULL )
      {
         printf("pclmulqdq_116 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_116 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x17a07657d2da7dd4ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x0545154178351fcaULL);
      }
   }
   else
   {
      printf("pclmulqdq_116 ... failed\n");
   }

   return;
}

static void pclmulqdq_117(void)
{
   reg128_t arg1 = { .uq = { 0x0ede1cf331ce42feULL, 0x076f0e7a7794e06eULL } };
   reg128_t arg2 = { .uq = { 0x03b7873e1a782f26ULL, 0x01dbc39febe9d682ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $0, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x964b33c978b91bc4ULL && result0.uq[1] == 0x0015145519fbe3f5ULL )
      {
         printf("pclmulqdq_117 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_117 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x964b33c978b91bc4ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x0015145519fbe3f5ULL);
      }
   }
   else
   {
      printf("pclmulqdq_117 ... failed\n");
   }

   return;
}

static void pclmulqdq_118(void)
{
   reg128_t arg1 = { .uq = { 0x00ede1d0d4a2aa30ULL, 0x0076f0e948ff1407ULL } };
   reg128_t arg2 = { .uq = { 0xc03b78758b2d48f0ULL, 0x601dbc3ba4446367ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $1, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x55314fda6f66cc90ULL && result0.uq[1] == 0x0026cec405b1e6f8ULL )
      {
         printf("pclmulqdq_118 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_118 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x55314fda6f66cc90ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x0026cec405b1e6f8ULL);
      }
   }
   else
   {
      printf("pclmulqdq_118 ... failed\n");
   }

   return;
}

static void pclmulqdq_119(void)
{
   reg128_t arg1 = { .uq = { 0xf00ede1eb8cff0a0ULL, 0x78076f103b15b73fULL } };
   reg128_t arg2 = { .uq = { 0xfc03b788f4389a8cULL, 0x7e01dbc558ca0c35ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $16, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x6a82fe6939f30c84ULL && result0.uq[1] == 0x28a26c46b7ebe635ULL )
      {
         printf("pclmulqdq_119 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_119 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x6a82fe6939f30c84ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x28a26c46b7ebe635ULL);
      }
   }
   else
   {
      printf("pclmulqdq_119 ... failed\n");
   }

   return;
}

static void pclmulqdq_120(void)
{
   reg128_t arg1 = { .uq = { 0xff00ede38312c507ULL, 0xbf8076f2a8372170ULL } };
   reg128_t arg2 = { .uq = { 0x5fc03b7a32c94fa7ULL, 0xefe01dbdf01266c0ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $17, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0xdcc216f4dd0dc400ULL && result0.uq[1] == 0x617576c564455645ULL )
      {
         printf("pclmulqdq_120 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_120 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0xdcc216f4dd0dc400ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x617576c564455645ULL);
      }
   }
   else
   {
      printf("pclmulqdq_120 ... failed\n");
   }

   return;
}

static void pclmulqdq_121(void)
{
   reg128_t arg1 = { .uq = { 0x77f00edfd6b6f24fULL, 0xfbf80770c2093814ULL } };
   reg128_t arg2 = { .uq = { 0x7dfc03b93fb25af9ULL, 0xfefe01dd7686ec6dULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $0, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x8bf569c40aba6f67ULL && result0.uq[1] == 0x1647572ea59fec88ULL )
      {
         printf("pclmulqdq_121 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_121 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x8bf569c40aba6f67ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x1647572ea59fec88ULL);
      }
   }
   else
   {
      printf("pclmulqdq_121 ... failed\n");
   }

   return;
}

static void pclmulqdq_122(void)
{
   reg128_t arg1 = { .uq = { 0xbf7f00ef91f13523ULL, 0x9fbf80789fa65982ULL } };
   reg128_t arg2 = { .uq = { 0x4fdfc03d2e80ebb0ULL, 0x27efe01f75ee34c7ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $1, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x1884b704d5d08ea9ULL && result0.uq[1] == 0x14b2b2b889275ed6ULL )
      {
         printf("pclmulqdq_122 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_122 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x1884b704d5d08ea9ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x14b2b2b889275ed6ULL);
      }
   }
   else
   {
      printf("pclmulqdq_122 ... failed\n");
   }

   return;
}

static void pclmulqdq_123(void)
{
   reg128_t arg1 = { .uq = { 0xd3f7f01091a4d950ULL, 0x69fbf80927802b97ULL } };
   reg128_t arg2 = { .uq = { 0xf4fdfc057a6dd4b8ULL, 0x7a7efe039be4a94bULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $16, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x2fa301cae17930a8ULL && result0.uq[1] == 0x24a3a8a439dad38bULL )
      {
         printf("pclmulqdq_123 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_123 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x2fa301cae17930a8ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x24a3a8a439dad38bULL);
      }
   }
   else
   {
      printf("pclmulqdq_123 ... failed\n");
   }

   return;
}

static void pclmulqdq_124(void)
{
   reg128_t arg1 = { .uq = { 0xfd3f7f02a4a01396ULL, 0x7e9fbf8230fdc8baULL } };
   reg128_t arg2 = { .uq = { 0x3f4fdfc1f72ca34cULL, 0x1fa7efe1da441095ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $17, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x5cff626dd6d19cf2ULL && result0.uq[1] == 0x0555105536974019ULL )
      {
         printf("pclmulqdq_124 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_124 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x5cff626dd6d19cf2ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x0555105536974019ULL);
      }
   }
   else
   {
      printf("pclmulqdq_124 ... failed\n");
   }

   return;
}

static void pclmulqdq_125(void)
{
   reg128_t arg1 = { .uq = { 0xcfd3f7f1c3cfc737ULL, 0xa7e9fbf9c895a288ULL } };
   reg128_t arg2 = { .uq = { 0x53f4fdfdc2f89033ULL, 0xe9fa7effc82a070aULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $0, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0xf6af04decad42cc9ULL && result0.uq[1] == 0x3e1bd743a16ab263ULL )
      {
         printf("pclmulqdq_125 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_125 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0xf6af04decad42cc9ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x3e1bd743a16ab263ULL);
      }
   }
   else
   {
      printf("pclmulqdq_125 ... failed\n");
   }

   return;
}

static void pclmulqdq_126(void)
{
   reg128_t arg1 = { .uq = { 0x74fd3f80c2c2c274ULL, 0x3a7e9fc1400f2029ULL } };
   reg128_t arg2 = { .uq = { 0xdd3f4fe186b54f05ULL, 0xae9fa7f1aa08666fULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $1, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x663f048b0f4c376cULL && result0.uq[1] == 0x3643329a1e33c49cULL )
      {
         printf("pclmulqdq_126 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_126 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x663f048b0f4c376cULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x3643329a1e33c49cULL);
      }
   }
   else
   {
      printf("pclmulqdq_126 ... failed\n");
   }

   return;
}

static void pclmulqdq_127(void)
{
   reg128_t arg1 = { .uq = { 0x974fd3f9bbb1f224ULL, 0x4ba7e9fdbc86b801ULL } };
   reg128_t arg2 = { .uq = { 0xe5d3f4ffb4f11af1ULL, 0xb2e9fa80b1264c69ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $16, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x2db9e522590922f1ULL && result0.uq[1] == 0x3f18ac8b499a5fdbULL )
      {
         printf("pclmulqdq_127 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_127 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x2db9e522590922f1ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x3f18ac8b499a5fdbULL);
      }
   }
   else
   {
      printf("pclmulqdq_127 ... failed\n");
   }

   return;
}

static void pclmulqdq_128(void)
{
   reg128_t arg1 = { .uq = { 0x9974fd412f40e525ULL, 0x8cba7ea17e4e317fULL } };
   reg128_t arg2 = { .uq = { 0x865d3f5195d4d7acULL, 0x432e9fa9a9982ac5ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $17, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x10a01544dcacf2c3ULL && result0.uq[1] == 0x22adc12d86b454a7ULL )
      {
         printf("pclmulqdq_128 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_128 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x10a01544dcacf2c3ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x22adc12d86b454a7ULL);
      }
   }
   else
   {
      printf("pclmulqdq_128 ... failed\n");
   }

   return;
}

static void pclmulqdq_129(void)
{
   reg128_t arg1 = { .uq = { 0xe1974fd5bb79d44fULL, 0xb0cba7ebb46aa914ULL } };
   reg128_t arg2 = { .uq = { 0x5865d3f6b8e31379ULL, 0xec32e9fc331f48adULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $0, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x39f8c415582c89e7ULL && result0.uq[1] == 0x31576a4bfbf9de3fULL )
      {
         printf("pclmulqdq_129 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_129 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x39f8c415582c89e7ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x31576a4bfbf9de3fULL);
      }
   }
   else
   {
      printf("pclmulqdq_129 ... failed\n");
   }

   return;
}

static void pclmulqdq_130(void)
{
   reg128_t arg1 = { .uq = { 0xb61974fef03d6343ULL, 0x9b0cba804ecc7092ULL } };
   reg128_t arg2 = { .uq = { 0x4d865d410613f738ULL, 0x26c32ea161b7ba8bULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $1, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0xfe6e5df6e325505dULL && result0.uq[1] == 0x150039e3f5d91dc7ULL )
      {
         printf("pclmulqdq_130 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_130 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0xfe6e5df6e325505dULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x150039e3f5d91dc7ULL);
      }
   }
   else
   {
      printf("pclmulqdq_130 ... failed\n");
   }

   return;
}

static void pclmulqdq_131(void)
{
   reg128_t arg1 = { .uq = { 0xd361975197899c36ULL, 0x69b0cba9aa728d0aULL } };
   reg128_t arg2 = { .uq = { 0x34d865d5b3e70574ULL, 0x1a6c32ebb8a141a9ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $16, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x51a65ce85f5f2548ULL && result0.uq[1] == 0x0a20a2805797ed23ULL )
      {
         printf("pclmulqdq_131 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_131 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x51a65ce85f5f2548ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x0a20a2805797ed23ULL);
      }
   }
   else
   {
      printf("pclmulqdq_131 ... failed\n");
   }

   return;
}

static void pclmulqdq_132(void)
{
   reg128_t arg1 = { .uq = { 0xcd361976b2fe5fc5ULL, 0xa69b0cbc302ceecfULL } };
   reg128_t arg2 = { .uq = { 0x934d865eeec43654ULL, 0x49a6c330560fda19ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $17, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x743fc6e863032247ULL && result0.uq[1] == 0x2c70b2e5c1075701ULL )
      {
         printf("pclmulqdq_132 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_132 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x743fc6e863032247ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x2c70b2e5c1075701ULL);
      }
   }
   else
   {
      printf("pclmulqdq_132 ... failed\n");
   }

   return;
}

static void pclmulqdq_133(void)
{
   reg128_t arg1 = { .uq = { 0xe4d3619901b5abfdULL, 0xb269b0cd678894ebULL } };
   reg128_t arg2 = { .uq = { 0x9934d8679a720966ULL, 0x4c9a6c34abe6c3a2ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $0, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0xe48d608e6c5c6deeULL && result0.uq[1] == 0x7bf7c8be42cbc359ULL )
      {
         printf("pclmulqdq_133 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_133 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0xe48d608e6c5c6deeULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x7bf7c8be42cbc359ULL);
      }
   }
   else
   {
      printf("pclmulqdq_133 ... failed\n");
   }

   return;
}

static void pclmulqdq_134(void)
{
   reg128_t arg1 = { .uq = { 0x264d361b34a120c0ULL, 0x13269b0e78fe4f4fULL } };
   reg128_t arg2 = { .uq = { 0xc9934d88132ce694ULL, 0x64c9a6c4e8443239ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $1, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x4e519b2288cbb2c0ULL && result0.uq[1] == 0x0dd8694f6a24e1f8ULL )
      {
         printf("pclmulqdq_134 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_134 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x4e519b2288cbb2c0ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x0dd8694f6a24e1f8ULL);
      }
   }
   else
   {
      printf("pclmulqdq_134 ... failed\n");
   }

   return;
}

static void pclmulqdq_135(void)
{
   reg128_t arg1 = { .uq = { 0xf264d3635acfd80dULL, 0xb93269b28415aaf3ULL } };
   reg128_t arg2 = { .uq = { 0x9c9934da28b8946aULL, 0x4e4c9a6df30a0924ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $16, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0xe72a61cb516c9cdeULL && result0.uq[1] == 0x50752c361fe9f790ULL )
      {
         printf("pclmulqdq_135 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_135 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0xe72a61cb516c9cdeULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x50752c361fe9f790ULL);
      }
   }
   else
   {
      printf("pclmulqdq_135 ... failed\n");
   }

   return;
}

static void pclmulqdq_136(void)
{
   reg128_t arg1 = { .uq = { 0x27264d37d832c381ULL, 0xd393269cc2c720b1ULL } };
   reg128_t arg2 = { .uq = { 0xa9c9934f48114f49ULL, 0x94e4c9a88ab66695ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $17, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x086a7585d4d637e5ULL && result0.uq[1] == 0x67faa7d867e596e8ULL )
      {
         printf("pclmulqdq_136 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_136 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x086a7585d4d637e5ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x67faa7d867e596e8ULL);
      }
   }
   else
   {
      printf("pclmulqdq_136 ... failed\n");
   }

   return;
}

static void pclmulqdq_137(void)
{
   reg128_t arg1 = { .uq = { 0x8a7264d52c08f237ULL, 0x8539326b7cb23808ULL } };
   reg128_t arg2 = { .uq = { 0x429c99369d06daf3ULL, 0xe14e4c9c25312c6aULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $0, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x4100ed2820bf0389ULL && result0.uq[1] == 0x23c3d0ef192c679bULL )
      {
         printf("pclmulqdq_137 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_137 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x4100ed2820bf0389ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x23c3d0ef192c679bULL);
      }
   }
   else
   {
      printf("pclmulqdq_137 ... failed\n");
   }

   return;
}

static void pclmulqdq_138(void)
{
   reg128_t arg1 = { .uq = { 0x70a7264ef1465524ULL, 0x385393285750e981ULL } };
   reg128_t arg2 = { .uq = { 0xdc29c995025633b1ULL, 0xae14e4cb67d8d8c9ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $1, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x8f6981733d8b4704ULL && result0.uq[1] == 0x34e77f6cd5d2d608ULL )
      {
         printf("pclmulqdq_138 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_138 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x8f6981733d8b4704ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x34e77f6cd5d2d608ULL);
      }
   }
   else
   {
      printf("pclmulqdq_138 ... failed\n");
   }

   return;
}

static void pclmulqdq_139(void)
{
   reg128_t arg1 = { .uq = { 0x970a72669a9a2b55ULL, 0x8b85393423fad497ULL } };
   reg128_t arg2 = { .uq = { 0x85c29c9af8ab2938ULL, 0x42e14e4e5b03538bULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $16, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x334171ba41a2b028ULL && result0.uq[1] == 0x470172dfe90ec7a3ULL )
      {
         printf("pclmulqdq_139 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_139 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x334171ba41a2b028ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x470172dfe90ec7a3ULL);
      }
   }
   else
   {
      printf("pclmulqdq_139 ... failed\n");
   }

   return;
}

static void pclmulqdq_140(void)
{
   reg128_t arg1 = { .uq = { 0xe170a728042f68b6ULL, 0x70b85394e0c5734aULL } };
   reg128_t arg2 = { .uq = { 0x385c29cb4f107894ULL, 0x1c2e14e68635fb39ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $17, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x464de85ad5b05afaULL && result0.uq[1] == 0x05401150a66ea4ddULL )
      {
         printf("pclmulqdq_140 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_140 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x464de85ad5b05afaULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x05401150a66ea4ddULL);
      }
   }
   else
   {
      printf("pclmulqdq_140 ... failed\n");
   }

   return;
}

static void pclmulqdq_141(void)
{
   reg128_t arg1 = { .uq = { 0xce170a7429c8bc8dULL, 0xa70b853afb921d33ULL } };
   reg128_t arg2 = { .uq = { 0x9385c29e5476cd8aULL, 0x49c2e15008e925b4ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $0, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x6f7ef804725bcaf2ULL && result0.uq[1] == 0x6adc6430b8465eafULL )
      {
         printf("pclmulqdq_141 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_141 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x6f7ef804725bcaf2ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x6adc6430b8465eafULL);
      }
   }
   else
   {
      printf("pclmulqdq_141 ... failed\n");
   }

   return;
}

static void pclmulqdq_142(void)
{
   reg128_t arg1 = { .uq = { 0x24e170a8e32251c9ULL, 0xd270b855583ee7d5ULL } };
   reg128_t arg2 = { .uq = { 0xa9385c2b82cd32d7ULL, 0x949c2e16a8145858ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $1, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x1be3bccbf3f77898ULL && result0.uq[1] == 0x10bca5dea41e786fULL )
      {
         printf("pclmulqdq_142 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_142 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x1be3bccbf3f77898ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x10bca5dea41e786fULL);
      }
   }
   else
   {
      printf("pclmulqdq_142 ... failed\n");
   }

   return;
}

static void pclmulqdq_143(void)
{
   reg128_t arg1 = { .uq = { 0x4a4e170c32b7eb1bULL, 0xe5270b86f009b47eULL } };
   reg128_t arg2 = { .uq = { 0x729385c456b2992eULL, 0x3949c2e30a070b86ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $16, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x2b68e2765ffdfb34ULL && result0.uq[1] == 0x2a08820822669a21ULL )
      {
         printf("pclmulqdq_143 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_143 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x2b68e2765ffdfb34ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x2a08820822669a21ULL);
      }
   }
   else
   {
      printf("pclmulqdq_143 ... failed\n");
   }

   return;
}

static void pclmulqdq_144(void)
{
   reg128_t arg1 = { .uq = { 0x1ca4e17263b144b2ULL, 0x0e5270ba10866148ULL } };
   reg128_t arg2 = { .uq = { 0x0729385de6f0ef93ULL, 0xc3949c2fda2636baULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $17, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x27bb3d7f7f6f6150ULL && result0.uq[1] == 0x04a8a03284562190ULL )
      {
         printf("pclmulqdq_144 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_144 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x27bb3d7f7f6f6150ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x04a8a03284562190ULL);
      }
   }
   else
   {
      printf("pclmulqdq_144 ... failed\n");
   }

   return;
}

static void pclmulqdq_145(void)
{
   reg128_t arg1 = { .uq = { 0x61ca4e18cbc0da4cULL, 0x30e5270d448e2c15ULL } };
   reg128_t arg2 = { .uq = { 0xd872938788f4d4f7ULL, 0xac3949c4ab282968ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $0, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x45795f55a307afa4ULL && result0.uq[1] == 0x2d97e09b0bc3ba6eULL )
      {
         printf("pclmulqdq_145 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_145 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x45795f55a307afa4ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x2d97e09b0bc3ba6eULL);
      }
   }
   else
   {
      printf("pclmulqdq_145 ... failed\n");
   }

   return;
}

static void pclmulqdq_146(void)
{
   reg128_t arg1 = { .uq = { 0x561ca4e33441d3a3ULL, 0xeb0e527270cea8c2ULL } };
   reg128_t arg2 = { .uq = { 0x7587293a17151350ULL, 0x3ac3949dea384897ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $1, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x8952066b3e0c09d9ULL && result0.uq[1] == 0x0d80def82390fa59ULL )
      {
         printf("pclmulqdq_146 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_146 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x8952066b3e0c09d9ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x0d80def82390fa59ULL);
      }
   }
   else
   {
      printf("pclmulqdq_146 ... failed\n");
   }

   return;
}

static void pclmulqdq_147(void)
{
   reg128_t arg1 = { .uq = { 0xdd61ca4fdbc9e338ULL, 0x6eb0e528cc92b08bULL } };
   reg128_t arg2 = { .uq = { 0xf75872954cf71736ULL, 0x7bac394b85294a8aULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $16, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x07e8e44eff28bbeaULL && result0.uq[1] == 0x26de695e1f9507acULL )
      {
         printf("pclmulqdq_147 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_147 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x07e8e44eff28bbeaULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x26de695e1f9507acULL);
      }
   }
   else
   {
      printf("pclmulqdq_147 ... failed\n");
   }

   return;
}

static void pclmulqdq_148(void)
{
   reg128_t arg1 = { .uq = { 0x3dd61ca6a1426434ULL, 0x1eeb0e542f4ef109ULL } };
   reg128_t arg2 = { .uq = { 0xcf75872afe553775ULL, 0xa7bac39655d85aa7ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $17, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x8fad3e20986ef89fULL && result0.uq[1] == 0x0cfdf3f1f7833e00ULL )
      {
         printf("pclmulqdq_148 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_148 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x8fad3e20986ef89fULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x0cfdf3f1f7833e00ULL);
      }
   }
   else
   {
      printf("pclmulqdq_148 ... failed\n");
   }

   return;
}

static void pclmulqdq_149(void)
{
   reg128_t arg1 = { .uq = { 0x93dd61cc0199ec40ULL, 0x49eeb0e6df7ab50fULL } };
   reg128_t arg2 = { .uq = { 0xe4f75874466b1974ULL, 0x727bac3b01e34ba9ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $0, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0xc93a243ecc35ad00ULL && result0.uq[1] == 0x7d58bcc299211eabULL )
      {
         printf("pclmulqdq_149 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_149 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0xc93a243ecc35ad00ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x7d58bcc299211eabULL);
      }
   }
   else
   {
      printf("pclmulqdq_149 ... failed\n");
   }

   return;
}

static void pclmulqdq_150(void)
{
   reg128_t arg1 = { .uq = { 0xf93dd61e679f64c5ULL, 0xbc9eeb101a7d714fULL } };
   reg128_t arg2 = { .uq = { 0x9e4f7588e3ec7794ULL, 0x4f27bac550a3fab9ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $1, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x96014936edcb669dULL && result0.uq[1] == 0x3b74884e3421a5a6ULL )
      {
         printf("pclmulqdq_150 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_150 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x96014936edcb669dULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x3b74884e3421a5a6ULL);
      }
   }
   else
   {
      printf("pclmulqdq_150 ... failed\n");
   }

   return;
}

static void pclmulqdq_151(void)
{
   reg128_t arg1 = { .uq = { 0xe793dd637effbc4dULL, 0xb3c9eeb2962d9d13ULL } };
   reg128_t arg2 = { .uq = { 0x99e4f75a21c48d7aULL, 0x4cf27badef9005acULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $16, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0xe083eb2842bea22eULL && result0.uq[1] == 0x579424d4aee04284ULL )
      {
         printf("pclmulqdq_151 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_151 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0xe083eb2842bea22eULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x579424d4aee04284ULL);
      }
   }
   else
   {
      printf("pclmulqdq_151 ... failed\n");
   }

   return;
}

static void pclmulqdq_152(void)
{
   reg128_t arg1 = { .uq = { 0x26793dd7d675c1c5ULL, 0xd33c9eecc1e89fcfULL } };
   reg128_t arg2 = { .uq = { 0xa99e4f7747a20ed4ULL, 0x54cf27bc827ec659ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $17, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0xbbe2a4fae75c8c87ULL && result0.uq[1] == 0x3ae9f530feca9e83ULL )
      {
         printf("pclmulqdq_152 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_152 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0xbbe2a4fae75c8c87ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x3ae9f530feca9e83ULL);
      }
   }
   else
   {
      printf("pclmulqdq_152 ... failed\n");
   }

   return;
}

static void pclmulqdq_153(void)
{
   reg128_t arg1 = { .uq = { 0xea6793df27ed221dULL, 0xb533c9f07aa44ffbULL } };
   reg128_t arg2 = { .uq = { 0x9a99e4f913ffe6eeULL, 0x4d4cf27d68adb266ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $0, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x2ecb87e2ee7738c6ULL && result0.uq[1] == 0x7d6e3e944eaff450ULL )
      {
         printf("pclmulqdq_153 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_153 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x2ecb87e2ee7738c6ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x7d6e3e944eaff450ULL);
      }
   }
   else
   {
      printf("pclmulqdq_153 ... failed\n");
   }

   return;
}

static void pclmulqdq_154(void)
{
   reg128_t arg1 = { .uq = { 0x26a6793f93049822ULL, 0x13533ca0a8300b00ULL } };
   reg128_t arg2 = { .uq = { 0x09a99e5132c5c46fULL, 0xc4d4cf297010a124ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $1, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0xae7428c8952c06c8ULL && result0.uq[1] == 0x1a782a535e1cf462ULL )
      {
         printf("pclmulqdq_154 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_154 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0xae7428c8952c06c8ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x1a782a535e1cf462ULL);
      }
   }
   else
   {
      printf("pclmulqdq_154 ... failed\n");
   }

   return;
}

static void pclmulqdq_155(void)
{
   reg128_t arg1 = { .uq = { 0x626a679596b60f81ULL, 0xf13533cba208c6b1ULL } };
   reg128_t arg2 = { .uq = { 0xb89a99e6b7b22249ULL, 0x9c4d4cf43286d015ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $16, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x4a8f4c6ff6f61d79ULL && result0.uq[1] == 0x6e57579e887d4fc8ULL )
      {
         printf("pclmulqdq_155 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_155 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x4a8f4c6ff6f61d79ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x6e57579e887d4fc8ULL);
      }
   }
   else
   {
      printf("pclmulqdq_155 ... failed\n");
   }

   return;
}

static void pclmulqdq_156(void)
{
   reg128_t arg1 = { .uq = { 0x8e26a67aeff126f7ULL, 0x8713533e5ea65268ULL } };
   reg128_t arg2 = { .uq = { 0x4389a9a00e00e823ULL, 0xe1c4d4d0edae3302ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%0, %%xmm12\n"
         "movhps 8+%0, %%xmm12\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $17, %%xmm12, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x7752ad4e77e19cd0ULL && result0.uq[1] == 0x72483d1e1c0e1d3aULL )
      {
         printf("pclmulqdq_156 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_156 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x7752ad4e77e19cd0ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x72483d1e1c0e1d3aULL);
      }
   }
   else
   {
      printf("pclmulqdq_156 ... failed\n");
   }

   return;
}

static void pclmulqdq_157(void)
{
   reg128_t arg1 = { .uq = { 0x70e26a695584d870ULL, 0x3871353589702b27ULL } };
   reg128_t arg2 = { .uq = { 0xdc389a9bab65d480ULL, 0x6e1c4d4eb460a92fULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $0, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0xd8c101b07d1ef800ULL && result0.uq[1] == 0x2109baafa5af4636ULL )
      {
         printf("pclmulqdq_157 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_157 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0xd8c101b07d1ef800ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x2109baafa5af4636ULL);
      }
   }
   else
   {
      printf("pclmulqdq_157 ... failed\n");
   }

   return;
}

static void pclmulqdq_158(void)
{
   reg128_t arg1 = { .uq = { 0xf70e26a830de1384ULL, 0x7b871354f71cc8b1ULL } };
   reg128_t arg2 = { .uq = { 0xfdc389ab523c2349ULL, 0xbee1c4d67fcbd095ULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $1, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x2e0ab5ce0af9e7d4ULL && result0.uq[1] == 0x6fc4778aacb0c279ULL )
      {
         printf("pclmulqdq_158 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_158 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x2e0ab5ce0af9e7d4ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x6fc4778aacb0c279ULL);
      }
   }
   else
   {
      printf("pclmulqdq_158 ... failed\n");
   }

   return;
}

static void pclmulqdq_159(void)
{
   reg128_t arg1 = { .uq = { 0x9f70e26c1693a737ULL, 0x8fb87136e1f79288ULL } };
   reg128_t arg2 = { .uq = { 0x47dc389c4fa98833ULL, 0xe3ee1c4f0e82830aULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $16, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0x5e76a6469d8b8e18ULL && result0.uq[1] == 0x202aa2a3ba5c9f52ULL )
      {
         printf("pclmulqdq_159 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_159 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0x5e76a6469d8b8e18ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x202aa2a3ba5c9f52ULL);
      }
   }
   else
   {
      printf("pclmulqdq_159 ... failed\n");
   }

   return;
}

static void pclmulqdq_160(void)
{
   reg128_t arg1 = { .uq = { 0x71f70e2865ef0074ULL, 0x38fb871511a53f29ULL } };
   reg128_t arg2 = { .uq = { 0xdc7dc38b5f805e85ULL, 0xae3ee1c6866dee2fULL } };
   reg128_t result0;
   char state[108];

   if (sigsetjmp(catchpoint, 1) == 0)
   {
      asm(
         "ffree %%st(7)\n"
         "ffree %%st(6)\n"
         "ffree %%st(5)\n"
         "ffree %%st(4)\n"
         "movlps 0+%1, %%xmm13\n"
         "movhps 8+%1, %%xmm13\n"
         "pclmulqdq $17, %0, %%xmm13\n"
         "movlps %%xmm13, 0+%2\n"
         "movhps %%xmm13, 8+%2\n"
         :
         : "m" (arg1), "m" (arg2), "m" (result0), "m" (state[0])
         : "xmm12", "xmm13"
      );

      if (result0.uq[0] == 0xe9f524d62e90ffb7ULL && result0.uq[1] == 0x1a32a63921dddceaULL )
      {
         printf("pclmulqdq_160 ... ok\n");
      }
      else
      {
         printf("pclmulqdq_160 ... not ok\n");
         printf("  result0.uq[0] = %llu (expected %llu)\n", result0.uq[0], 0xe9f524d62e90ffb7ULL);
         printf("  result0.uq[1] = %llu (expected %llu)\n", result0.uq[1], 0x1a32a63921dddceaULL);
      }
   }
   else
   {
      printf("pclmulqdq_160 ... failed\n");
   }

   return;
}

int main(int argc, char **argv)
{
   signal(SIGILL, handle_sigill);

   pclmulqdq_1();
   pclmulqdq_2();
   pclmulqdq_3();
   pclmulqdq_4();
   pclmulqdq_5();
   pclmulqdq_6();
   pclmulqdq_7();
   pclmulqdq_8();
   pclmulqdq_9();
   pclmulqdq_10();
   pclmulqdq_11();
   pclmulqdq_12();
   pclmulqdq_13();
   pclmulqdq_14();
   pclmulqdq_15();
   pclmulqdq_16();
   pclmulqdq_17();
   pclmulqdq_18();
   pclmulqdq_19();
   pclmulqdq_20();
   pclmulqdq_21();
   pclmulqdq_22();
   pclmulqdq_23();
   pclmulqdq_24();
   pclmulqdq_25();
   pclmulqdq_26();
   pclmulqdq_27();
   pclmulqdq_28();
   pclmulqdq_29();
   pclmulqdq_30();
   pclmulqdq_31();
   pclmulqdq_32();
   pclmulqdq_33();
   pclmulqdq_34();
   pclmulqdq_35();
   pclmulqdq_36();
   pclmulqdq_37();
   pclmulqdq_38();
   pclmulqdq_39();
   pclmulqdq_40();
   pclmulqdq_41();
   pclmulqdq_42();
   pclmulqdq_43();
   pclmulqdq_44();
   pclmulqdq_45();
   pclmulqdq_46();
   pclmulqdq_47();
   pclmulqdq_48();
   pclmulqdq_49();
   pclmulqdq_50();
   pclmulqdq_51();
   pclmulqdq_52();
   pclmulqdq_53();
   pclmulqdq_54();
   pclmulqdq_55();
   pclmulqdq_56();
   pclmulqdq_57();
   pclmulqdq_58();
   pclmulqdq_59();
   pclmulqdq_60();
   pclmulqdq_61();
   pclmulqdq_62();
   pclmulqdq_63();
   pclmulqdq_64();
   pclmulqdq_65();
   pclmulqdq_66();
   pclmulqdq_67();
   pclmulqdq_68();
   pclmulqdq_69();
   pclmulqdq_70();
   pclmulqdq_71();
   pclmulqdq_72();
   pclmulqdq_73();
   pclmulqdq_74();
   pclmulqdq_75();
   pclmulqdq_76();
   pclmulqdq_77();
   pclmulqdq_78();
   pclmulqdq_79();
   pclmulqdq_80();
   pclmulqdq_81();
   pclmulqdq_82();
   pclmulqdq_83();
   pclmulqdq_84();
   pclmulqdq_85();
   pclmulqdq_86();
   pclmulqdq_87();
   pclmulqdq_88();
   pclmulqdq_89();
   pclmulqdq_90();
   pclmulqdq_91();
   pclmulqdq_92();
   pclmulqdq_93();
   pclmulqdq_94();
   pclmulqdq_95();
   pclmulqdq_96();
   pclmulqdq_97();
   pclmulqdq_98();
   pclmulqdq_99();
   pclmulqdq_100();
   pclmulqdq_101();
   pclmulqdq_102();
   pclmulqdq_103();
   pclmulqdq_104();
   pclmulqdq_105();
   pclmulqdq_106();
   pclmulqdq_107();
   pclmulqdq_108();
   pclmulqdq_109();
   pclmulqdq_110();
   pclmulqdq_111();
   pclmulqdq_112();
   pclmulqdq_113();
   pclmulqdq_114();
   pclmulqdq_115();
   pclmulqdq_116();
   pclmulqdq_117();
   pclmulqdq_118();
   pclmulqdq_119();
   pclmulqdq_120();
   pclmulqdq_121();
   pclmulqdq_122();
   pclmulqdq_123();
   pclmulqdq_124();
   pclmulqdq_125();
   pclmulqdq_126();
   pclmulqdq_127();
   pclmulqdq_128();
   pclmulqdq_129();
   pclmulqdq_130();
   pclmulqdq_131();
   pclmulqdq_132();
   pclmulqdq_133();
   pclmulqdq_134();
   pclmulqdq_135();
   pclmulqdq_136();
   pclmulqdq_137();
   pclmulqdq_138();
   pclmulqdq_139();
   pclmulqdq_140();
   pclmulqdq_141();
   pclmulqdq_142();
   pclmulqdq_143();
   pclmulqdq_144();
   pclmulqdq_145();
   pclmulqdq_146();
   pclmulqdq_147();
   pclmulqdq_148();
   pclmulqdq_149();
   pclmulqdq_150();
   pclmulqdq_151();
   pclmulqdq_152();
   pclmulqdq_153();
   pclmulqdq_154();
   pclmulqdq_155();
   pclmulqdq_156();
   pclmulqdq_157();
   pclmulqdq_158();
   pclmulqdq_159();
   pclmulqdq_160();

   exit(0);
}

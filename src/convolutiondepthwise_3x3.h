static void convdw3x3s1_neon(const Mat& bottom_blob, Mat& top_blob, const Mat& _kernel, const Mat& _bias, const Option& opt)
{
    int w = bottom_blob.w;

    int outw = top_blob.w;
    int outh = top_blob.h;

    const int group = bottom_blob.c;

    const float* kernel = _kernel;
    const float* bias = _bias;

    #pragma omp parallel for num_threads(opt.num_threads)
    for (int g = 0; g < group; g++)
    {
        Mat out = top_blob.channel(g);

        const float bias0 = bias ? bias[g] : 0.f;

        const float* kernel0 = kernel + g * 9;

        float* outptr = out;
        float* outptr2 = outptr + outw;

        const float* img0 = bottom_blob.channel(g);

        const float* r0 = img0;
        const float* r1 = img0 + w;
        const float* r2 = img0 + w * 2;
        const float* r3 = img0 + w * 3;

#if __ARM_NEON
        float32x4_t _k012x = vld1q_f32(kernel0);
        float32x4_t _k345x = vld1q_f32(kernel0 + 3);
        float32x4_t _k678x = vld1q_f32(kernel0 + 6);

        _k012x = vsetq_lane_f32(0.f, _k012x, 3);
        _k345x = vsetq_lane_f32(0.f, _k345x, 3);
        _k678x = vsetq_lane_f32(0.f, _k678x, 3);

        float32x4_t _bias0 = vdupq_n_f32(bias0);
#else
        const float* k0 = kernel0;
        const float* k1 = kernel0 + 3;
        const float* k2 = kernel0 + 6;
#endif // __ARM_NEON

        int i = 0;

        for (; i + 1 < outh; i += 2)
        {
#if __ARM_NEON
#if __aarch64__
            int nn = outw >> 3;
            int remain = outw & 7;
#else
            int nn = outw >> 2;
            int remain = outw & 3;
#endif // __aarch64__
#else
            int remain = outw;
#endif // __ARM_NEON

#if __ARM_NEON
            if (nn > 0)
            {
                asm volatile(
                    "pld        [%3, #192]          \n"
                    "vld1.f32   {d18-d20}, [%3 :64] \n" // r0
                    "add        %3, #16             \n"

                    "vext.32    q11, q9, q10, #1    \n"
                    "vext.32    q12, q9, q10, #2    \n"

                    "0:                             \n"

                    "vmul.f32   q7, q9, %e14[0]     \n"

                    "vand       q13, %q17, %q17     \n" // q13 = _bias0
                    "vmul.f32   q6, q11, %e14[1]    \n"
                    "vmla.f32   q13, q12, %f14[0]   \n"

                    "pld        [%4, #192]          \n"
                    "vld1.f32   {d18-d20}, [%4]     \n" // r1
                    "add        %4, #16             \n"

                    "vmla.f32   q7, q9, %e15[0]     \n"

                    "vext.32    q11, q9, q10, #1    \n"
                    "vext.32    q12, q9, q10, #2    \n"

                    "vmla.f32   q6, q11, %e15[1]    \n"
                    "vmla.f32   q13, q12, %f15[0]   \n"

                    "vmul.f32   q8, q9, %e14[0]     \n"

                    "vand       q15, %q17, %q17     \n" // q15 = _bias0
                    "vmul.f32   q14, q11, %e14[1]   \n"
                    "vmla.f32   q15, q12, %f14[0]   \n"

                    "pld        [%5, #192]          \n"
                    "vld1.f32   {d18-d20}, [%5 :64] \n" // r2
                    "add        %5, #16             \n"

                    "vmla.f32   q7, q9, %e16[0]     \n"

                    "vext.32    q11, q9, q10, #1    \n"
                    "vext.32    q12, q9, q10, #2    \n"

                    "vmla.f32   q6, q11, %e16[1]    \n"
                    "vmla.f32   q13, q12, %f16[0]   \n"

                    "vmla.f32   q8, q9, %e15[0]     \n"
                    "vmla.f32   q14, q11, %e15[1]   \n"
                    "vmla.f32   q15, q12, %f15[0]   \n"

                    "pld        [%6, #192]          \n"
                    "vld1.f32   {d18-d20}, [%6]     \n" // r3
                    "add        %6, #16             \n"

                    "vmla.f32   q8, q9, %e16[0]     \n"

                    "vext.32    q11, q9, q10, #1    \n"
                    "vext.32    q12, q9, q10, #2    \n"

                    "vmla.f32   q14, q11, %e16[1]   \n"
                    "vmla.f32   q15, q12, %f16[0]   \n"

                    "vadd.f32   q7, q7, q6          \n"

                    "pld        [%3, #192]          \n"
                    "vld1.f32   {d18-d20}, [%3 :64] \n" // r0

                    "vadd.f32   q8, q8, q14         \n"
                    "vadd.f32   q7, q7, q13         \n"
                    "vadd.f32   q8, q8, q15         \n"

                    "vext.32    q11, q9, q10, #1    \n"
                    "vext.32    q12, q9, q10, #2    \n"

                    "add        %3, #16             \n"

                    "vst1.f32   {d14-d15}, [%1]!    \n"
                    "vst1.f32   {d16-d17}, [%2]!    \n"

                    "subs       %0, #1              \n"
                    "bne        0b                  \n"

                    "sub        %3, #16             \n"
                    : "=r"(nn),      // %0
                    "=r"(outptr),  // %1
                    "=r"(outptr2), // %2
                    "=r"(r0),      // %3
                    "=r"(r1),      // %4
                    "=r"(r2),      // %5
                    "=r"(r3)       // %6
                    : "0"(nn),
                    "1"(outptr),
                    "2"(outptr2),
                    "3"(r0),
                    "4"(r1),
                    "5"(r2),
                    "6"(r3),
                    "w"(_k012x), // %14
                    "w"(_k345x), // %15
                    "w"(_k678x), // %16
                    "w"(_bias0)  // %17
                    : "cc", "memory", "q6", "q7", "q8", "q9", "q10", "q11", "q12", "q13", "q14", "q15");
            }
#endif // __aarch64__
#endif // __ARM_NEON
            for (; remain > 0; remain--)
            {
#if __ARM_NEON
                float32x4_t _r00 = vld1q_f32(r0);
                float32x4_t _r10 = vld1q_f32(r1);
                float32x4_t _r20 = vld1q_f32(r2);
                float32x4_t _r30 = vld1q_f32(r3);

                float32x4_t _sum = vmulq_f32(_r00, _k012x);
                _sum = vmlaq_f32(_sum, _r10, _k345x);
                _sum = vmlaq_f32(_sum, _r20, _k678x);

                float32x4_t _sum2 = vmulq_f32(_r10, _k012x);
                _sum2 = vmlaq_f32(_sum2, _r20, _k345x);
                _sum2 = vmlaq_f32(_sum2, _r30, _k678x);

                _sum = vsetq_lane_f32(bias0, _sum, 3);
                _sum2 = vsetq_lane_f32(bias0, _sum2, 3);
#if __aarch64__
                *outptr = vaddvq_f32(_sum);
                *outptr2 = vaddvq_f32(_sum2);
#else
                float32x2_t _ss = vadd_f32(vget_low_f32(_sum), vget_high_f32(_sum));
                float32x2_t _ss2 = vadd_f32(vget_low_f32(_sum2), vget_high_f32(_sum2));

                float32x2_t _sss2 = vpadd_f32(_ss, _ss2);

                *outptr = vget_lane_f32(_sss2, 0);
                *outptr2 = vget_lane_f32(_sss2, 1);
#endif // __aarch64__
#else
                float sum = bias0;
                sum += r0[0] * k0[0];
                sum += r0[1] * k0[1];
                sum += r0[2] * k0[2];
                sum += r1[0] * k1[0];
                sum += r1[1] * k1[1];
                sum += r1[2] * k1[2];
                sum += r2[0] * k2[0];
                sum += r2[1] * k2[1];
                sum += r2[2] * k2[2];

                float sum2 = bias0;
                sum2 += r1[0] * k0[0];
                sum2 += r1[1] * k0[1];
                sum2 += r1[2] * k0[2];
                sum2 += r2[0] * k1[0];
                sum2 += r2[1] * k1[1];
                sum2 += r2[2] * k1[2];
                sum2 += r3[0] * k2[0];
                sum2 += r3[1] * k2[1];
                sum2 += r3[2] * k2[2];

                *outptr = sum;
                *outptr2 = sum2;
#endif
                r0++;
                r1++;
                r2++;
                r3++;
                outptr++;
                outptr2++;
            }

            r0 += 2 + w;
            r1 += 2 + w;
            r2 += 2 + w;
            r3 += 2 + w;

            outptr += outw;
            outptr2 += outw;
        }

        for (; i < outh; i++)
        {
#if __ARM_NEON
#if __aarch64__
            int nn = outw >> 3;
            int remain = outw & 7;
#else
            int nn = outw >> 2;
            int remain = outw & 3;
#endif // __aarch64__
#else
            int remain = outw;
#endif // __ARM_NEON

#if __ARM_NEON
            if (nn > 0)
            {
                asm volatile(
                    "pld        [%2, #192]          \n"
                    "vld1.f32   {d16-d18}, [%2]     \n" // r0
                    "add        %2, #16             \n"

                    "vext.32    q10, q8, q9, #1     \n"
                    "vext.32    q11, q8, q9, #2     \n"

                    "0:                             \n"

                    "vmul.f32   q7, q8, %e10[0]     \n"

                    "vand       q14, %q13, %q13     \n" // q14 = _bias0
                    "vmul.f32   q13, q10, %e10[1]   \n"
                    "vmla.f32   q14, q11, %f10[0]   \n"

                    "pld        [%3, #192]          \n"
                    "vld1.f32   {d16-d18}, [%3]     \n" // r1
                    "add        %3, #16             \n"

                    "vmla.f32   q7, q8, %e11[0]     \n"

                    "vext.32    q10, q8, q9, #1     \n"
                    "vext.32    q11, q8, q9, #2     \n"

                    "vmla.f32   q13, q10, %e11[1]   \n"
                    "vmla.f32   q14, q11, %f11[0]   \n"

                    "pld        [%4, #192]          \n"
                    "vld1.f32   {d16-d18}, [%4]     \n" // r2
                    "add        %4, #16             \n"

                    "vmla.f32   q7, q8, %e12[0]     \n"

                    "vext.32    q10, q8, q9, #1     \n"
                    "vext.32    q11, q8, q9, #2     \n"

                    "vmla.f32   q13, q10, %e12[1]   \n"
                    "vmla.f32   q14, q11, %f12[0]   \n"

                    "pld        [%2, #192]          \n"
                    "vld1.f32   {d16-d18}, [%2]     \n" // r0
                    "add        %2, #16             \n"

                    "vadd.f32   q7, q7, q13         \n"
                    "vadd.f32   q7, q7, q14         \n"

                    "vext.32    q10, q8, q9, #1     \n"
                    "vext.32    q11, q8, q9, #2     \n"

                    "vst1.f32   {d14-d15}, [%1]!    \n"

                    "subs       %0, #1              \n"
                    "bne        0b                  \n"

                    "sub        %2, #16             \n"
                    : "=r"(nn),     // %0
                    "=r"(outptr), // %1
                    "=r"(r0),     // %2
                    "=r"(r1),     // %3
                    "=r"(r2)      // %4
                    : "0"(nn),
                    "1"(outptr),
                    "2"(r0),
                    "3"(r1),
                    "4"(r2),
                    "w"(_k012x), // %10
                    "w"(_k345x), // %11
                    "w"(_k678x), // %12
                    "w"(_bias0)  // %13
                    : "cc", "memory", "q7", "q8", "q9", "q10", "q11", "q12", "q13", "q14", "q15");
            }
#endif // __aarch64__
#endif // __ARM_NEON
            for (; remain > 0; remain--)
            {
#if __ARM_NEON
                float32x4_t _r00 = vld1q_f32(r0);
                float32x4_t _r10 = vld1q_f32(r1);
                float32x4_t _r20 = vld1q_f32(r2);

                float32x4_t _sum = vmulq_f32(_r00, _k012x);
                _sum = vmlaq_f32(_sum, _r10, _k345x);
                _sum = vmlaq_f32(_sum, _r20, _k678x);

                _sum = vsetq_lane_f32(bias0, _sum, 3);
#if __aarch64__
                *outptr = vaddvq_f32(_sum);
#else
                float32x2_t _ss = vadd_f32(vget_low_f32(_sum), vget_high_f32(_sum));
                _ss = vpadd_f32(_ss, _ss);

                *outptr = vget_lane_f32(_ss, 0);
#endif // __aarch64__
#else
                float sum = bias0;
                sum += r0[0] * k0[0];
                sum += r0[1] * k0[1];
                sum += r0[2] * k0[2];
                sum += r1[0] * k1[0];
                sum += r1[1] * k1[1];
                sum += r1[2] * k1[2];
                sum += r2[0] * k2[0];
                sum += r2[1] * k2[1];
                sum += r2[2] * k2[2];

                *outptr = sum;
#endif
                r0++;
                r1++;
                r2++;
                outptr++;
            }

            r0 += 2;
            r1 += 2;
            r2 += 2;
        }
    }
}

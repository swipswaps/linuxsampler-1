/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2008-2009 Andreas Persson                               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the Free Software           *
 *   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,                *
 *   MA  02110-1301  USA                                                   *
 ***************************************************************************/

#ifndef LSATOMIC_H
#define LSATOMIC_H

/*
 * Implementation of a small subset of the C++0x atomic operations
 * (cstdatomic).
 *
 * The supported operations are:
 *
 * - fences (acquire, release and seq_cst)
 *
 * - load and store of atomic<int> with relaxed, acquire/release or
 *   seq_cst memory ordering
 *
 * The supported architectures are x86 and powerpc.
 */
namespace LinuxSampler {
    enum memory_order {
        memory_order_relaxed, memory_order_acquire,
        memory_order_release, memory_order_seq_cst
    };

    inline void atomic_thread_fence(memory_order order) {
        switch (order) {
        case memory_order_relaxed:
            break;

        case memory_order_acquire:
        case memory_order_release:
#ifdef _ARCH_PPC64
            asm volatile("lwsync" : : : "memory");
#elif defined(_ARCH_PPC)
            asm volatile("sync" : : : "memory");
#else
            asm volatile("" : : : "memory");
#endif
            break;

        case memory_order_seq_cst:
#ifdef _ARCH_PPC
            asm volatile("sync" : : : "memory");
#elif defined(__i386__)
            asm volatile("lock; addl $0,0(%%esp)" : : : "memory");
#elif defined(__x86_64__)
            asm volatile("mfence" : : : "memory");
#else
            asm volatile("" : : : "memory");
#endif
            break;
        }
    }

    template<typename T> class atomic;
    template<> class atomic<int> { // int is the only implemented type
    public:
        atomic() { }
        explicit atomic(int m) : f(m) { }
        int load(memory_order order = memory_order_seq_cst) const volatile {
            int m;
            switch (order) {
            case memory_order_relaxed:
                m = f;
                break;

            case memory_order_seq_cst:
            case memory_order_release: // (invalid)
                atomic_thread_fence(memory_order_seq_cst);
                // fall-through

            case memory_order_acquire:
#ifdef _ARCH_PPC
                // PPC load-acquire: artificial dependency + isync
                asm volatile(
                    "lwz%U1%X1 %0,%1\n\t"
                    "cmpw %0,%0\n\t"
                    "bne- 1f\n\t"
                    "1: isync"
                    : "=r" (m)
                    : "m"  (f)
                    : "memory", "cr0");
#else
                m = f;
                asm volatile("" : : : "memory");
#endif
                break;
            }
            return m;
        }

        void store(int m, memory_order order = memory_order_seq_cst) volatile {
            switch (order) {
            case memory_order_relaxed:
                f = m;
                break;

            case memory_order_release:
                atomic_thread_fence(memory_order_release);
                f = m;
                break;

            case memory_order_seq_cst:
            case memory_order_acquire: // (invalid)
                atomic_thread_fence(memory_order_release);
                f = m;
                atomic_thread_fence(memory_order_seq_cst);
                break;
            }
        }
    private:
        int f;
        atomic(const atomic&); // not allowed
        atomic& operator=(const atomic&); // not allowed
    };
}
#endif

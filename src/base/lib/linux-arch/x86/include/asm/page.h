#ifndef _ASM_X86_PAGE_H
#define _ASM_X86_PAGE_H

#include <linux/types.h>

#ifdef __KERNEL__

#if 0 // CapROS
#include <asm/page_types.h>

#ifdef CONFIG_X86_64
#include <asm/page_64.h>
#else
#include <asm/page_32.h>
#endif	/* CONFIG_X86_64 */

#ifndef __ASSEMBLY__

struct page;

static inline void clear_user_page(void *page, unsigned long vaddr,
				   struct page *pg)
{
	clear_page(page);
}

static inline void copy_user_page(void *to, void *from, unsigned long vaddr,
				  struct page *topage)
{
	copy_page(to, from);
}

#define __alloc_zeroed_user_highpage(movableflags, vma, vaddr) \
	alloc_page_vma(GFP_HIGHUSER | __GFP_ZERO | movableflags, vma, vaddr)
#define __HAVE_ARCH_ALLOC_ZEROED_USER_HIGHPAGE

#define __pa(x)		__phys_addr((unsigned long)(x))
#define __pa_nodebug(x)	__phys_addr_nodebug((unsigned long)(x))
/* __pa_symbol should be used for C visible symbols.
   This seems to be the official gcc blessed way to do such arithmetic. */
#define __pa_symbol(x)	__pa(__phys_reloc_hide((unsigned long)(x)))

#define __va(x)			((void *)((unsigned long)(x)+PAGE_OFFSET))

#define __boot_va(x)		__va(x)
#define __boot_pa(x)		__pa(x)

/*
 * virt_to_page(kaddr) returns a valid pointer if and only if
 * virt_addr_valid(kaddr) returns true.
 */
#define virt_to_page(kaddr)	pfn_to_page(__pa(kaddr) >> PAGE_SHIFT)
#define pfn_to_kaddr(pfn)      __va((pfn) << PAGE_SHIFT)
extern bool __virt_addr_valid(unsigned long kaddr);
#define virt_addr_valid(kaddr)	__virt_addr_valid((unsigned long) (kaddr))

#endif	/* __ASSEMBLY__ */

#include <asm-generic/memory_model.h>
#include <asm-generic/page.h>

#define __HAVE_ARCH_GATE_AREA 1
#endif // CapROS

#endif	/* __KERNEL__ */
#endif /* _ASM_X86_PAGE_H */

/*
* Viola kernel fault handler
* File: viola_fault.c
*
* Copyright (c) 2016 University of California, Irvine, CA, USA
* All rights reserved.
*
* Authors: Saeed Mirzamohammadi <saeed@uci.edu>
*          Ardalan Amiri Sani <arrdalan@gmail.com>
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License version 2 as published by
* the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License along with
* this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <linux/module.h>
#include <net/sock.h>
#include <linux/proc_fs.h>
#include <asm/cacheflush.h> 
#include <asm/tlbflush.h>
#include <asm/highmem.h>
#include <asm/delay.h>
#include <linux/prints.h>
#include <linux/vmalloc.h>
#include <linux/viola.h>


#ifdef CONFIG_ARM
PTE_BIT_FUNC(exprotect, |= L_PTE_XN);
PTE_BIT_FUNC(mkpresent, |= L_PTE_PRESENT);
PTE_BIT_FUNC(mknotpresent, &= ~L_PTE_PRESENT); // use for invalidation (to signal NO permissions)
#define dfv_pte_present pte_present

#define LPRINTK0(fmt, args...)

pte_t *viola_get_pte(struct mm_struct *mm, unsigned long addr)
{
	pgd_t *pgd;

	if (!mm)
		mm = &init_mm;

	LPRINTK0(KERN_ALERT "pgd = %p\n", mm->pgd);
	pgd = pgd_offset(mm, addr);
	LPRINTK0(KERN_ALERT "[%08lx] *pgd=%08llx",
			addr, (long long)pgd_val(*pgd));

	do {
		pud_t *pud;
		pmd_t *pmd;
		pte_t *pte;

		if (pgd_none(*pgd))
			break;

		pud = pud_offset(pgd, addr);
		if (PTRS_PER_PUD != 1)
			LPRINTK0(", *pud=%08lx", pud_val(*pud));

		if (pud_none(*pud))
			break;

		pmd = pmd_offset(pud, addr);
		if (PTRS_PER_PMD != 1)
			LPRINTK0(", *pmd=%08llx", (long long)pmd_val(*pmd));

		if (pmd_none(*pmd))
			break;

		/* We must not map this if we have highmem enabled */
		if (PageHighMem(pfn_to_page(pmd_val(*pmd) >> PAGE_SHIFT)))
			break;

		pte = pte_offset_map(pmd, addr);
		return pte;
	} while(0);

	return NULL;
}


#endif /* CONFIG_ARM */

int viola_change_page_state(unsigned long local_addr, int state)
{
	pte_t *ptep = viola_get_pte(NULL, local_addr);
		
	if (!ptep)
		goto error_no_pte;
			
	switch (state) {
		
	case VIOLA_SHARED:
		/* grant read-only permissions to the PTE, aka SHARED state */
		set_pte_ext(ptep, pte_mkpresent(*ptep), 0);
		set_pte_ext(ptep, pte_wrprotect(*ptep), 0);
		break;
		
	case VIOLA_MODIFIED:
		set_pte_ext(ptep, pte_mkpresent(*ptep), 0);
		set_pte_ext(ptep, pte_mkwrite(*ptep), 0);
		break;		
		
	case VIOLA_INVALID:
	      set_pte_ext(ptep, pte_mknotpresent(*ptep), 0);
	      set_pte_ext(ptep, pte_wrprotect(*ptep), 0);
		break;
		
	default:
		PRINTK_ERR("Error: unknown state.\n");
		break;
	}
	flush_tlb_all();
		
	pte_unmap(ptep);
	return 0;
	
error_no_pte:
	PRINTK_ERR("Error: PTE is NULL \n");
	return -EFAULT;
}
EXPORT_SYMBOL(viola_change_page_state);

int viola_kernel_fault(struct mm_struct *mm, unsigned long addr, unsigned int fsr,
	    	       struct pt_regs *regs)
{
	char pc_1_val;
	int src_reg_index; 
	char reg;
	char char_val;
	char devid;
	unsigned long dst_addr;
	bool ignore = false;

	if ((addr >= 0xf9016000) & (addr < 0xf9017000)) {
		devid = (char) 0x5;
		goto cont;
	} else if ((addr >= 0xf9017000) & (addr < 0xf9018000)) {
		devid = (char) 0x4;
		goto cont;
	} else if ((addr >= 0xfa018000) & (addr < 0xfa019000)) {
		devid = (char) 0x6;
		/*
		 * this condition and the next similar one is due to a limitation that
		 * we currently have that register offsets can only be up to 0xff.
		 * We have adjusted the spec as well.
		 */
		if (addr < 0xfa018300)
			ignore = true;
		goto cont;
	} else if ((addr >= 0xf901b000) & (addr < 0xf901c000)) {
		devid = (char) 0x7;
		if (addr < 0xf901b400)
			ignore = true;
		goto cont;
	} else {
		goto err_nomem2;
	}

cont:
		pc_1_val = *((char *) (regs->ARM_pc + 1));
		src_reg_index = pc_1_val & 0xf0; 
		src_reg_index = src_reg_index >> 4;
		reg = addr & 0xff;
		char_val = (char) regs->uregs[src_reg_index];

		if (ignore)
			goto emulate;

		if (viola_check_reg_access(devid, reg, char_val)) {
			PRINTK_ERR("[4.1]: Error: not allowed by Viola.\n");
			goto err_nomem;
		}

		if (regs->uregs[src_reg_index] > 0xff) {
			if (reg == 0xff) {
				PRINTK_ERR("Error: something wrong happened\n");
				goto err_nomem;
			}
			char_val = (char) ((regs->uregs[src_reg_index] >> 8) & 0xff);
			reg = reg + 1;
			if (viola_check_reg_access(devid, reg, char_val)) {
				PRINTK_ERR("[4.3]: Error: not allowed by Viola.\n");
				goto err_nomem;
			}
		}

emulate:
err_nomem:
		dst_addr = addr + 0x700000;
		PRINTK0("[5]: dst_addr = %#x\n", (unsigned int) dst_addr);
		writel(regs->uregs[src_reg_index], dst_addr);
	
		regs->ARM_pc += 4;
		LPRINTK0("[5]: regs->ARM_pc = %#x\n", (unsigned int) regs->ARM_pc);
	return 0;

err_nomem2:
	return -ENOMEM;
}

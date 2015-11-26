#ifndef _VIOLA_H_
#define _VIOLA_H_

#define VIOLA_SHARED	0
#define VIOLA_MODIFIED	1
#define VIOLA_INVALID	2

int viola_change_page_state(unsigned long local_addr, int state);
int viola_kernel_fault(struct mm_struct *mm, unsigned long addr, unsigned int fsr,
	    	       struct pt_regs *regs);
int viola_check_reg_access(char devid, char regoff, char regval);


//int vibrator_pwm_set(int enable, int amp, int n_value);
int g_msm8974_pwm_vibrator_force_set(int gain, int n_value);
/* for testing */
int g_vibrator_ic_enable_set(int enable);
#endif /* _VIOAL_H_ */

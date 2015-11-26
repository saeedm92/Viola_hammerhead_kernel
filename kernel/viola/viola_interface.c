/*
* Viola kernel interface
* File: viola_interface.c
*
* Copyright (c) 2016 University of California, Irvine, CA, USA
* All rights reserved.
*
* Authors: Saeed Mirzamohammadi <saeed@uci.edu>
* 	   Ardalan Amiri Sani <arrdalan@gmail.com>
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

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/prints.h>
#include <linux/module.h>

int viola_main_ic_clk(int master_iomem_ptr, int slave_iomem_ptr, int regaccess_ptr);
int viola_main_vib_ic(int master_iomem_ptr, int slave_iomem_ptr, int regaccess_ptr);
int viola_main_cam_vib(int master_iomem_ptr, int slave_iomem_ptr, int regaccess_ptr);

bool iomems_initialized = false;
char *cam_iomem;
char *vib_iomem;
char *vib_ic_iomem;
char *vib_clk_iomem;

static int init_iomem(unsigned int devid)
{
	if (devid == 0x4) { /* cam */
		cam_iomem = kzalloc(256 * sizeof(char), GFP_KERNEL);
		if (!cam_iomem) {
			PRINTK_ERR("Error: could not allocate cam_iomem\n");
			return -ENOMEM;
		}
	}

	if (devid == 0x5) { /* vib */
		vib_iomem = kzalloc(256 * sizeof(char), GFP_KERNEL);
		if (!vib_iomem) {
			PRINTK_ERR("Error: could not allocate vib_iomem\n");
			return -ENOMEM;
		}
	}

	if (devid == 0x6) { /* vib ic */
		vib_ic_iomem = kzalloc(256 * sizeof(char), GFP_KERNEL);
		if (!vib_ic_iomem) {
			PRINTK_ERR("Error: could not allocate vib_ic_iomem\n");
			return -ENOMEM;
		}
	}

	if (devid == 0x7) { /* vib clk */
		vib_clk_iomem = kzalloc(256 * sizeof(char), GFP_KERNEL);
		if (!vib_clk_iomem) {
			PRINTK_ERR("Error: could not allocate vib_clk_iomem\n");
			return -ENOMEM;
		}
	}

	return 0;
}

int viola_check_reg_access(char devid, char regoff, char regval)
{
	char regaccess[3];
	int ret = 0;

	if (!iomems_initialized) {
		init_iomem(0x4); //cam
		init_iomem(0x5); //galaxy nexus led
		init_iomem(0x6);
		init_iomem(0x7);
#ifdef IN_KERNEL
		iomems_initialized = true;
#else
		iomems_initialized = 1;
#endif
	}

	regaccess[0] = devid;
	regaccess[1] = regoff;
	regaccess[2] = regval;


	ret = viola_main_ic_clk((int) vib_ic_iomem, (int) vib_clk_iomem, (int) regaccess);
	if (ret) {
		PRINTK_ERR("Error: Not allowed by viola_main_ic_clk\n");
		goto err;
	}
	
	ret = viola_main_vib_ic((int) vib_iomem, (int) vib_ic_iomem, (int) regaccess);
	if (ret) {
		PRINTK_ERR("Error: Not allowed by viola_main_vib_ic\n");
		goto err;
	}
	
	ret = viola_main_cam_vib((int) cam_iomem, (int) vib_iomem, (int) regaccess);
	if (ret) {
		PRINTK_ERR("Error: Not allowed by viola_main_cam_vib\n");
		goto err;
	}
		if (devid == 0x5) {
			vib_iomem[(int) regoff] = (char) regval;
		}
		if (devid == 0x4) { /* cam */
			cam_iomem[(int) regoff] = (char) regval;
		}
		if (devid == 0x6) { /* vib ic */
			vib_ic_iomem[(int) regoff] = (char) regval;
		}
		if (devid == 0x7) { /* vib clk */
			vib_clk_iomem[(int) regoff] = (char) regval;
		}
err:
	return ret;

}
EXPORT_SYMBOL(viola_check_reg_access);


#ifndef __INGENIC_SFC_COM_H__
#define __INGENIC_SFC_COM_H__
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/mtd/mtd.h>
#include <soc/sfc.h>

struct cmd_info{
	int cmd;
	int cmd_len;/*reserved; not use*/
	int dataen;
	int sta_exp;
	int sta_msk;
};

struct sfc_transfer {
	int direction;

	struct cmd_info *cmd_info;

	int addr_len;
	unsigned int addr;
	unsigned int addr_plus;
	int addr_dummy_bits;/*cmd + addr_dummy_bits + addr*/

	const unsigned char *data;
	int data_dummy_bits;/*addr + data_dummy_bits + data*/
	unsigned int len;
	unsigned int cur_len;

	int sfc_mode;
	int ops_mode;
	int phase_format;/*we just use default value;phase1:cmd+dummy+addr... phase0:cmd+addr+dummy...*/

	struct list_head transfer_list;
};

struct sfc_message {
	struct list_head    transfers;
	unsigned        actual_length;
	int         status;

};

struct sfc{

	void __iomem            *iomem;
	struct resource     *ioarea;
	int                     irq;
	struct clk              *clk;
	struct clk              *clk_gate;
	unsigned long src_clk;
	struct completion       done;
	int                     threshold;
	irqreturn_t (*irq_callback)(int irq, void* dev);
	unsigned long           phys;

	struct sfc_transfer *transfer;
};


struct sfc_flash;

struct spi_nor_flash_ops {
	int (*set_4byte_mode)(struct sfc_flash *flash);
	int (*set_quad_mode)(struct sfc_flash *flash);
};


struct sfc_flash {
	struct mtd_info     mtd;
	struct device           *dev;
	struct resource         *resource;
	struct sfc                      *sfc;
	void *flash_info;
	unsigned int flash_info_num;

	struct mutex        lock;

	int                     status;
	spinlock_t              lock_status;

	int quad_mode;
	int quad_succeed;
	struct spi_nor_info *g_nor_info;
	struct spi_nor_flash_ops        *nor_flash_ops;
	struct spi_nor_cmd_info  *cur_r_cmd;
	struct spi_nor_cmd_info  *cur_w_cmd;
	struct burner_params *params;
	struct norflash_partitions *norflash_partitions;
};

void dump_sfc_reg(struct sfc *sfc);

void sfc_message_init(struct sfc_message *m);
void sfc_message_add_tail(struct sfc_transfer *t, struct sfc_message *m);
void sfc_transfer_del(struct sfc_transfer *t);
int sfc_sync(struct sfc *sfc, struct sfc_message *message);
int sfc_res_init(struct platform_device *pdev, struct sfc_flash *flash);
unsigned int sfc_get_sta_rt(struct sfc *sfc);
int set_flash_timing(struct sfc *sfc, unsigned int t_hold, unsigned int t_setup, unsigned int t_shslrd, unsigned int t_shslwr);

int sfc_nor_get_special_ops(struct sfc_flash *flash);

#endif



// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2026 Han Gao <gaohan@iscas.ac.cn> */

#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/reset.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi-mem.h>

#define SPIF_VERSION			0x00
#define SPIF_GC				0x04
#define SPIF_GC_CFG_MODE		BIT(0)
#define SPIF_GC_NMODE_EN		BIT(2)
#define SPIF_GC_CPHA			BIT(4)
#define SPIF_GC_CPOL			BIT(5)
#define SPIF_GC_CS_SEL			GENMASK(7, 6)
#define SPIF_GC_CS_POL			BIT(8)
#define SPIF_GCA			0x08
#define SPIF_GCA_FIFO_SRST		GENMASK(1, 0)
#define SPIF_INT_EN			0x14
#define SPIF_INT_STA			0x18
#define SPIF_INT_FIFO_ERRORS		GENMASK(11, 8)
#define SPIF_INT_DMA_DONE		BIT(24)
#define SPIF_INT_SPI_DONE		BIT(26)
#define SPIF_CS_DELAY			0x1c
#define SPIF_CS_DELAY_CSSOT		GENMASK(7, 0)
#define SPIF_CS_DELAY_CSEOT		GENMASK(15, 8)
#define SPIF_CS_DELAY_CSDA		GENMASK(23, 16)
#define SPIF_TRANS_PHA			0x20
#define SPIF_TRANS_PHA_RX		BIT(8)
#define SPIF_TRANS_PHA_TX		BIT(12)
#define SPIF_TRANS_PHA_DUMMY		BIT(16)
#define SPIF_TRANS_PHA_ADDR		BIT(24)
#define SPIF_TRANS_PHA_CMD		BIT(28)
#define SPIF_TRANS_CFG1			0x24
#define SPIF_TRANS_CFG2			0x28
#define SPIF_TRANS_CFG2_CMD		GENMASK(31, 24)
#define SPIF_TRANS_NUM			0x2c
#define SPIF_TRANS_NUM_DATA		GENMASK(15, 0)
#define SPIF_TRANS_NUM_DUMMY		GENMASK(23, 16)
#define SPIF_TRANS_NUM_ADDR_SIZE	GENMASK(25, 24)
#define SPIF_TRANS_NUM_NORMAL_EN	BIT(28)
#define SPIF_DMA_CTRL			0x40
#define SPIF_DMA_CTRL_START		BIT(0)
#define SPIF_DMA_CTRL_DESC_LEN		GENMASK(11, 4)
#define SPIF_DESC_SADDR			0x44

#define SPIF_DESC_FINISH		BIT(0)
#define SPIF_DESC_DMA_WRITE		BIT(1)
#define SPIF_DESC_HBURST		GENMASK(6, 4)
#define SPIF_DESC_HBURST_INCR16		7
#define SPIF_DESC_DATA_LEN		GENMASK(16, 0)
#define SPIF_DESC_BLK_LEN		GENMASK(31, 24)
#define SPIF_DESC_BLK_LEN_64B		3

#define SPIF_BUFFER_SIZE		4096

/* Words 4 to 7 load the SPI_TRANS_* registers. */
struct sunxi_spif_desc {
	__le32 ctrl;
	__le32 len;
	__le32 buf_addr;
	__le32 next;
	__le32 trans_pha;
	__le32 trans_cfg1;
	__le32 trans_cfg2;
	__le32 trans_num;
};

struct sunxi_spif {
	struct device *dev;
	void __iomem *base;
	struct clk *bus_clk;
	struct clk *mod_clk;
	struct reset_control *rst;
	struct completion done;
	struct sunxi_spif_desc *desc;
	u8 *buffer;
	u32 expected;
	u32 status;
	unsigned int speed;
	int irq;
	bool clocks_on;
	bool failed;
};

/* The controller soft reset does not stop a pending descriptor chain. */
static int sunxi_spif_init_hw(struct sunxi_spif *spif)
{
	int ret;

	ret = reset_control_assert(spif->rst);
	if (ret)
		return ret;
	ret = reset_control_deassert(spif->rst);
	if (ret)
		return ret;
	if (readl(spif->base + SPIF_VERSION) < 0x10002)
		return -ENODEV;
	/* Keep at least six clocks between two chip select periods. */
	writel(FIELD_PREP(SPIF_CS_DELAY_CSDA, 5) |
	       FIELD_PREP(SPIF_CS_DELAY_CSEOT, 6) |
	       FIELD_PREP(SPIF_CS_DELAY_CSSOT, 6), spif->base + SPIF_CS_DELAY);
	return 0;
}

static void sunxi_spif_disable(void *data)
{
	struct sunxi_spif *spif = data;

	if (!spif->clocks_on)
		return;
	writel(0, spif->base + SPIF_INT_EN);
	reset_control_assert(spif->rst);
	clk_disable_unprepare(spif->mod_clk);
	clk_disable_unprepare(spif->bus_clk);
	spif->clocks_on = false;
}

static irqreturn_t sunxi_spif_irq(int irq, void *data)
{
	struct sunxi_spif *spif = data;
	u32 status = readl(spif->base + SPIF_INT_STA) &
		     readl(spif->base + SPIF_INT_EN);

	if (!status)
		return IRQ_NONE;
	writel(status, spif->base + SPIF_INT_STA);
	spif->status |= status;
	if ((spif->status & spif->expected) == spif->expected ||
	    (spif->status & SPIF_INT_FIFO_ERRORS)) {
		writel(0, spif->base + SPIF_INT_EN);
		complete(&spif->done);
	}
	return IRQ_HANDLED;
}

static int sunxi_spif_wait(struct sunxi_spif *spif)
{
	int ret = 0;

	if (!wait_for_completion_timeout(&spif->done, msecs_to_jiffies(1000)))
		ret = -ETIMEDOUT;
	else if (spif->status & SPIF_INT_FIFO_ERRORS)
		ret = -EIO;
	writel(0, spif->base + SPIF_INT_EN);
	synchronize_irq(spif->irq);
	/* Do not hand a buffer that the DMA engine may still write back. */
	if (ret && sunxi_spif_init_hw(spif)) {
		sunxi_spif_disable(spif);
		spif->failed = true;
	}
	return ret;
}

static bool sunxi_spif_supports_op(struct spi_mem *mem, const struct spi_mem_op *op)
{
	if (op->cmd.nbytes != 1 || op->cmd.buswidth != 1 ||
	    op->addr.nbytes > 4 || op->addr.val > U32_MAX ||
	    (op->addr.nbytes && op->addr.buswidth != 1) ||
	    op->dummy.nbytes > 31 ||
	    (op->dummy.nbytes && op->dummy.buswidth != 1) ||
	    (op->data.nbytes && op->data.buswidth != 1))
		return false;

	return spi_mem_default_supports_op(mem, op);
}

static int sunxi_spif_adjust_op_size(struct spi_mem *mem, struct spi_mem_op *op)
{
	op->data.nbytes = min_t(unsigned int, op->data.nbytes, SPIF_BUFFER_SIZE);
	return 0;
}

static int sunxi_spif_exec_op(struct spi_mem *mem, const struct spi_mem_op *op)
{
	struct sunxi_spif *spif = spi_controller_get_devdata(mem->spi->controller);
	struct sunxi_spif_desc *desc = spif->desc;
	enum dma_data_direction direction = op->data.dir == SPI_MEM_DATA_IN ?
		DMA_FROM_DEVICE : DMA_TO_DEVICE;
	dma_addr_t buffer, descriptor;
	unsigned int speed = mem->spi->max_speed_hz;
	u32 control, ctrl, phase, number;
	int ret;

	if (spif->failed)
		return -EIO;
	if (!sunxi_spif_supports_op(mem, op) || op->data.nbytes > SPIF_BUFFER_SIZE)
		return -EOPNOTSUPP;
	if (op->max_freq)
		speed = min(speed, op->max_freq);
	if (!speed)
		return -EINVAL;
	speed = min(speed, 50000000U);
	if (speed != spif->speed) {
		ret = clk_set_rate(spif->mod_clk, speed);
		if (ret)
			return ret;
		if (clk_get_rate(spif->mod_clk) > speed)
			return -ERANGE;
		spif->speed = speed;
	}

	phase = SPIF_TRANS_PHA_CMD;
	number = FIELD_PREP(SPIF_TRANS_NUM_DATA, op->data.nbytes);
	if (op->addr.nbytes) {
		phase |= SPIF_TRANS_PHA_ADDR;
		number |= FIELD_PREP(SPIF_TRANS_NUM_ADDR_SIZE, op->addr.nbytes - 1);
	}
	if (op->dummy.nbytes) {
		phase |= SPIF_TRANS_PHA_DUMMY;
		number |= FIELD_PREP(SPIF_TRANS_NUM_DUMMY, op->dummy.nbytes * 8);
	}
	control = SPIF_GC_CS_POL |
		  FIELD_PREP(SPIF_GC_CS_SEL, spi_get_chipselect(mem->spi, 0));
	if (mem->spi->mode & SPI_CPHA)
		control |= SPIF_GC_CPHA;
	if (mem->spi->mode & SPI_CPOL)
		control |= SPIF_GC_CPOL;
	writel(readl(spif->base + SPIF_GCA) | SPIF_GCA_FIFO_SRST,
	       spif->base + SPIF_GCA);
	writel(SPIF_INT_SPI_DONE | SPIF_INT_DMA_DONE | SPIF_INT_FIFO_ERRORS,
	       spif->base + SPIF_INT_STA);
	reinit_completion(&spif->done);
	spif->status = 0;

	/*
	 * A descriptor chain without data never raises DMA completion and
	 * blocks the next chain, so take these parameters from registers.
	 */
	if (!op->data.nbytes) {
		spif->expected = SPIF_INT_SPI_DONE;
		writel(control, spif->base + SPIF_GC);
		writel(phase, spif->base + SPIF_TRANS_PHA);
		writel(op->addr.val, spif->base + SPIF_TRANS_CFG1);
		writel(FIELD_PREP(SPIF_TRANS_CFG2_CMD, op->cmd.opcode),
		       spif->base + SPIF_TRANS_CFG2);
		writel(number, spif->base + SPIF_TRANS_NUM);
		writel(spif->expected | SPIF_INT_FIFO_ERRORS, spif->base + SPIF_INT_EN);
		writel(control | SPIF_GC_NMODE_EN, spif->base + SPIF_GC);
		return sunxi_spif_wait(spif);
	}

	ctrl = SPIF_DESC_FINISH |
	       FIELD_PREP(SPIF_DESC_HBURST, SPIF_DESC_HBURST_INCR16);
	if (op->data.dir == SPI_MEM_DATA_IN) {
		phase |= SPIF_TRANS_PHA_RX;
		ctrl |= SPIF_DESC_DMA_WRITE;
	} else {
		phase |= SPIF_TRANS_PHA_TX;
		memcpy(spif->buffer, op->data.buf.out, op->data.nbytes);
	}
	buffer = dma_map_single(spif->dev, spif->buffer, op->data.nbytes, direction);
	if (dma_mapping_error(spif->dev, buffer))
		return -ENOMEM;
	desc->ctrl = cpu_to_le32(ctrl);
	desc->len = cpu_to_le32(FIELD_PREP(SPIF_DESC_BLK_LEN, SPIF_DESC_BLK_LEN_64B) |
				FIELD_PREP(SPIF_DESC_DATA_LEN, op->data.nbytes));
	desc->buf_addr = cpu_to_le32(buffer >> 2);
	desc->next = 0;
	desc->trans_pha = cpu_to_le32(phase);
	desc->trans_cfg1 = cpu_to_le32(op->addr.val);
	desc->trans_cfg2 = cpu_to_le32(FIELD_PREP(SPIF_TRANS_CFG2_CMD, op->cmd.opcode));
	desc->trans_num = cpu_to_le32(SPIF_TRANS_NUM_NORMAL_EN | number);
	descriptor = dma_map_single(spif->dev, desc, sizeof(*desc), DMA_TO_DEVICE);
	if (dma_mapping_error(spif->dev, descriptor)) {
		ret = -ENOMEM;
		goto unmap_buffer;
	}

	/* DMA completion alone does not show that the bus cycle ended. */
	spif->expected = SPIF_INT_SPI_DONE | SPIF_INT_DMA_DONE;
	writel(control | SPIF_GC_CFG_MODE, spif->base + SPIF_GC);
	writel(descriptor >> 2, spif->base + SPIF_DESC_SADDR);
	writel(spif->expected | SPIF_INT_FIFO_ERRORS, spif->base + SPIF_INT_EN);
	writel(FIELD_PREP(SPIF_DMA_CTRL_DESC_LEN, sizeof(*desc)) |
	       SPIF_DMA_CTRL_START, spif->base + SPIF_DMA_CTRL);
	ret = sunxi_spif_wait(spif);
	dma_unmap_single(spif->dev, descriptor, sizeof(*desc), DMA_TO_DEVICE);
unmap_buffer:
	dma_unmap_single(spif->dev, buffer, op->data.nbytes, direction);
	if (!ret && op->data.dir == SPI_MEM_DATA_IN)
		memcpy(op->data.buf.in, spif->buffer, op->data.nbytes);
	return ret;
}

static const struct spi_controller_mem_ops sunxi_spif_mem_ops = {
	.supports_op = sunxi_spif_supports_op,
	.adjust_op_size = sunxi_spif_adjust_op_size,
	.exec_op = sunxi_spif_exec_op,
};

static int sunxi_spif_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct spi_controller *ctlr;
	struct sunxi_spif *spif;
	int ret;

	ctlr = devm_spi_alloc_host(dev, sizeof(*spif));
	if (!ctlr)
		return -ENOMEM;
	spif = spi_controller_get_devdata(ctlr);
	spif->dev = dev;
	spif->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(spif->base))
		return PTR_ERR(spif->base);
	spif->bus_clk = devm_clk_get(dev, "ahb");
	if (IS_ERR(spif->bus_clk))
		return dev_err_probe(dev, PTR_ERR(spif->bus_clk), "getting bus clock\n");
	spif->mod_clk = devm_clk_get(dev, "mod");
	if (IS_ERR(spif->mod_clk))
		return dev_err_probe(dev, PTR_ERR(spif->mod_clk), "getting module clock\n");
	spif->rst = devm_reset_control_get_exclusive(dev, NULL);
	if (IS_ERR(spif->rst))
		return dev_err_probe(dev, PTR_ERR(spif->rst), "getting reset\n");
	ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(32));
	if (ret)
		return ret;
	/* Separate cacheline-aligned allocations avoid DMA ownership sharing. */
	spif->desc = devm_kzalloc(dev, ALIGN(sizeof(*spif->desc), dma_get_cache_alignment()),
				  GFP_KERNEL);
	spif->buffer = devm_kmalloc(dev, SPIF_BUFFER_SIZE, GFP_KERNEL);
	if (!spif->desc || !spif->buffer)
		return -ENOMEM;
	init_completion(&spif->done);
	spif->irq = platform_get_irq(pdev, 0);
	if (spif->irq < 0)
		return spif->irq;
	ret = clk_prepare_enable(spif->bus_clk);
	if (ret)
		return ret;
	ret = clk_prepare_enable(spif->mod_clk);
	if (ret) {
		clk_disable_unprepare(spif->bus_clk);
		return ret;
	}
	spif->clocks_on = true;
	ret = devm_add_action_or_reset(dev, sunxi_spif_disable, spif);
	if (ret)
		return ret;
	ret = sunxi_spif_init_hw(spif);
	if (ret)
		return ret;
	ret = devm_request_irq(dev, spif->irq, sunxi_spif_irq, 0, dev_name(dev), spif);
	if (ret)
		return ret;
	ctlr->mode_bits = SPI_CPOL | SPI_CPHA;
	ctlr->num_chipselect = 4;
	ctlr->max_speed_hz = 50000000;
	ctlr->mem_ops = &sunxi_spif_mem_ops;
	ctlr->dev.of_node = dev->of_node;
	return devm_spi_register_controller(dev, ctlr);
}

static const struct of_device_id sunxi_spif_match[] = {
	{ .compatible = "allwinner,sun252i-v861-spif" },
	{ }
};
MODULE_DEVICE_TABLE(of, sunxi_spif_match);

static struct platform_driver sunxi_spif_driver = {
	.probe = sunxi_spif_probe,
	.driver = {
		.name = "sunxi-spif",
		.of_match_table = sunxi_spif_match,
	},
};
module_platform_driver(sunxi_spif_driver);

MODULE_DESCRIPTION("Allwinner dedicated SPI flash controller");
MODULE_LICENSE("GPL");

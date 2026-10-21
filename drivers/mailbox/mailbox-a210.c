// SPDX-License-Identifier: GPL-2.0-only
/* AP to E902 doorbell channel on the Zhihe A210. */
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/mailbox_controller.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>

#define A210_MBOX_STATUS		0x00
#define A210_MBOX_CLEAR		0x04
#define A210_MBOX_MASK		0x0c
#define A210_MBOX_GENERATE	0x10
#define A210_MBOX_E902_PENDING	BIT(0)
#define A210_MBOX_NOTIFY		BIT(6)

struct a210_mbox {
	struct mbox_controller controller;
	struct mbox_chan channel;
	void __iomem *local;
	void __iomem *remote;
	struct clk_bulk_data clocks[2];
	/* Protects this channel's interrupt mask against concurrent updates. */
	spinlock_t lock;
	int irq;
};

static void a210_mbox_mask(struct a210_mbox *mbox, bool enable)
{
	unsigned long flags;
	u32 value;

	spin_lock_irqsave(&mbox->lock, flags);
	value = readl(mbox->local + A210_MBOX_MASK);
	if (enable)
		value |= A210_MBOX_E902_PENDING;
	else
		value &= ~A210_MBOX_E902_PENDING;
	writel(value, mbox->local + A210_MBOX_MASK);
	spin_unlock_irqrestore(&mbox->lock, flags);
}

static irqreturn_t a210_mbox_irq(int irq, void *data)
{
	struct a210_mbox *mbox = data;

	if (!(readl(mbox->local + A210_MBOX_MASK) & A210_MBOX_E902_PENDING))
		return IRQ_NONE;
	if (!(readl(mbox->local + A210_MBOX_STATUS) & A210_MBOX_E902_PENDING))
		return IRQ_NONE;

	writel(A210_MBOX_E902_PENDING, mbox->local + A210_MBOX_CLEAR);
	mbox_chan_received_data(&mbox->channel, NULL);
	return IRQ_HANDLED;
}

static int a210_mbox_send(struct mbox_chan *channel, void *data)
{
	struct a210_mbox *mbox = channel->con_priv;

	/* Payload and completion ownership belong to the shared-memory client. */
	writel(A210_MBOX_NOTIFY, mbox->remote + A210_MBOX_GENERATE);
	return 0;
}

static int a210_mbox_startup(struct mbox_chan *channel)
{
	struct a210_mbox *mbox = channel->con_priv;

	writel(A210_MBOX_E902_PENDING, mbox->local + A210_MBOX_CLEAR);
	a210_mbox_mask(mbox, true);
	return 0;
}

static void a210_mbox_shutdown(struct mbox_chan *channel)
{
	struct a210_mbox *mbox = channel->con_priv;

	a210_mbox_mask(mbox, false);
	synchronize_irq(mbox->irq);
}

static const struct mbox_chan_ops a210_mbox_ops = {
	.send_data = a210_mbox_send,
	.startup = a210_mbox_startup,
	.shutdown = a210_mbox_shutdown,
};

static void a210_mbox_disable_clocks(void *data)
{
	struct a210_mbox *mbox = data;

	clk_bulk_disable_unprepare(ARRAY_SIZE(mbox->clocks), mbox->clocks);
}

static int a210_mbox_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct a210_mbox *mbox;
	int ret;

	mbox = devm_kzalloc(dev, sizeof(*mbox), GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;

	mbox->local = devm_platform_ioremap_resource_byname(pdev, "interrupt");
	if (IS_ERR(mbox->local))
		return PTR_ERR(mbox->local);
	mbox->remote = devm_platform_ioremap_resource_byname(pdev, "tx");
	if (IS_ERR(mbox->remote))
		return PTR_ERR(mbox->remote);
	mbox->irq = platform_get_irq(pdev, 0);
	if (mbox->irq < 0)
		return mbox->irq;

	mbox->clocks[0].id = "local";
	mbox->clocks[1].id = "remote";
	ret = devm_clk_bulk_get(dev, ARRAY_SIZE(mbox->clocks), mbox->clocks);
	if (ret)
		return dev_err_probe(dev, ret, "failed to get mailbox clocks\n");
	ret = clk_bulk_prepare_enable(ARRAY_SIZE(mbox->clocks), mbox->clocks);
	if (ret)
		return ret;
	ret = devm_add_action_or_reset(dev, a210_mbox_disable_clocks, mbox);
	if (ret)
		return ret;

	spin_lock_init(&mbox->lock);
	a210_mbox_mask(mbox, false);
	mbox->channel.con_priv = mbox;
	mbox->controller.dev = dev;
	mbox->controller.chans = &mbox->channel;
	mbox->controller.num_chans = 1;
	mbox->controller.ops = &a210_mbox_ops;
	/* The client acknowledges completion after consuming shared memory. */
	ret = devm_request_irq(dev, mbox->irq, a210_mbox_irq, IRQF_SHARED,
			       dev_name(dev), mbox);
	if (ret)
		return ret;

	return devm_mbox_controller_register(dev, &mbox->controller);
}

static const struct of_device_id a210_mbox_match[] = {
	{ .compatible = "zhihe,a210-ap-mailbox" },
	{}
};
MODULE_DEVICE_TABLE(of, a210_mbox_match);

static struct platform_driver a210_mbox_driver = {
	.probe = a210_mbox_probe,
	.driver = {
		.name = "a210-mailbox",
		.of_match_table = a210_mbox_match,
		/* Running E902 firmware continuously accesses the remote bank. */
		.suppress_bind_attrs = true,
	},
};
module_platform_driver(a210_mbox_driver);

MODULE_AUTHOR("Han Gao <gaohan@iscas.ac.cn>");
MODULE_DESCRIPTION("Zhihe A210 AP to E902 doorbell");
MODULE_LICENSE("GPL");

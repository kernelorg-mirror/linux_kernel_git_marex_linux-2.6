/*
 * Microchip KSZ8795 series register access through SPI
 *
 * Copyright (C) 2017 Microchip Technology Inc.
 *	Tristram Ha <Tristram.Ha@microchip.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <asm/unaligned.h>

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spi/spi.h>

#include "ksz_priv.h"
#include "ksz_spi.h"

/* SPI frame opcodes */
#define KS_SPIOP_RD			3
#define KS_SPIOP_WR			2

#define SPI_ADDR_S			12
#define SPI_ADDR_M			(BIT(SPI_ADDR_S) - 1)
#define SPI_TURNAROUND_S		1

/* Enough to read all switch registers. */
#define SPI_TX_BUF_LEN			0x100

static int ksz8795_spi_read_reg(struct spi_device *spi, u32 reg, u8 *val,
				unsigned int len)
{
	u16 txbuf;
	int ret;

	txbuf = reg & SPI_ADDR_M;
	txbuf |= KS_SPIOP_RD << SPI_ADDR_S;
	txbuf <<= SPI_TURNAROUND_S;
	txbuf = cpu_to_be16(txbuf);

	ret = spi_write_then_read(spi, &txbuf, 2, val, len);
	return ret;
}

static int ksz8795_spi_write_reg(struct spi_device *spi, u32 reg, u8 *val,
				 unsigned int len)
{
	u16 *txbuf = (u16 *)val;

	*txbuf = reg & SPI_ADDR_M;
	*txbuf |= (KS_SPIOP_WR << SPI_ADDR_S);
	*txbuf <<= SPI_TURNAROUND_S;
	*txbuf = cpu_to_be16(*txbuf);

	return spi_write(spi, txbuf, 2 + len);
}

static int ksz_spi_read(struct ksz_device *dev, u32 reg, u8 *data,
			unsigned int len)
{
	struct spi_device *spi = dev->priv;

	return ksz8795_spi_read_reg(spi, reg, data, len);
}

static int ksz_spi_write(struct ksz_device *dev, u32 reg, void *data,
			 unsigned int len)
{
	struct spi_device *spi = dev->priv;

	if (len > SPI_TX_BUF_LEN)
		len = SPI_TX_BUF_LEN;
	memcpy(&dev->txbuf[2], data, len);
	return ksz8795_spi_write_reg(spi, reg, dev->txbuf, len);
}

static const struct ksz_io_ops ksz8795_spi_ops = {
	.read8 = ksz_spi_read8,
	.read16 = ksz_spi_read16,
	.read32 = ksz_spi_read32,
	.write8 = ksz_spi_write8,
	.write16 = ksz_spi_write16,
	.write32 = ksz_spi_write32,
	.get = ksz_spi_get,
	.set = ksz_spi_set,
};

static int ksz8795_spi_probe(struct spi_device *spi)
{
	struct ksz_device *dev;
	int ret;

	dev = ksz_switch_alloc(&spi->dev, &ksz8795_spi_ops, spi);
	if (!dev)
		return -ENOMEM;

	if (spi->dev.platform_data)
		dev->pdata = spi->dev.platform_data;

	dev->txbuf = devm_kzalloc(dev->dev, 2 + SPI_TX_BUF_LEN, GFP_KERNEL);

	ret = ksz8795_switch_register(dev);

	/* Main DSA driver may not be started yet. */
	if (ret)
		return ret;

	spi_set_drvdata(spi, dev);

	return 0;
}

static int ksz8795_spi_remove(struct spi_device *spi)
{
	struct ksz_device *dev = spi_get_drvdata(spi);

	if (dev)
		ksz_switch_remove(dev);

	return 0;
}

static void ksz8795_spi_shutdown(struct spi_device *spi)
{
	struct ksz_device *dev = spi_get_drvdata(spi);

	if (dev && dev->dev_ops->shutdown)
		dev->dev_ops->shutdown(dev);
}

static const struct of_device_id ksz8795_dt_ids[] = {
	{ .compatible = "microchip,ksz8795" },
	{ .compatible = "microchip,ksz8794" },
	{ .compatible = "microchip,ksz8765" },
	{},
};
MODULE_DEVICE_TABLE(of, ksz8795_dt_ids);

static struct spi_driver ksz8795_spi_driver = {
	.driver = {
		.name	= "ksz8795-switch",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(ksz8795_dt_ids),
	},
	.probe	= ksz8795_spi_probe,
	.remove	= ksz8795_spi_remove,
	.shutdown = ksz8795_spi_shutdown,
};

module_spi_driver(ksz8795_spi_driver);

MODULE_AUTHOR("Tristram Ha <Tristram.Ha@microchip.com>");
MODULE_DESCRIPTION("Microchip KSZ8795 Series Switch SPI Driver");
MODULE_LICENSE("GPL");

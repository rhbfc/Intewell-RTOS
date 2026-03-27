/* SPDX-License-Identifier: Apache-2.0 */
#ifndef _UAPI_SPI_H
#define _UAPI_SPI_H

/*
 * SPI mode and transfer flags exposed to userspace.
 * Keep the bit assignments stable to preserve the UAPI.
 */

#define SPI_CPHA		(1UL << 0)	/* clock phase */
#define SPI_CPOL		(1UL << 1)	/* clock polarity */

#define SPI_MODE_0		0U		/* (original MicroWire) */
#define SPI_MODE_1		SPI_CPHA
#define SPI_MODE_2		SPI_CPOL
#define SPI_MODE_3		(SPI_CPOL | SPI_CPHA)
#define SPI_MODE_X_MASK		(SPI_CPOL | SPI_CPHA)

#define SPI_CS_HIGH		(1UL << 2)	/* chipselect active high? */
#define SPI_LSB_FIRST		(1UL << 3)	/* per-word bits-on-wire */
#define SPI_3WIRE		(1UL << 4)	/* SI/SO signals shared */
#define SPI_LOOP		(1UL << 5)	/* loopback mode */
#define SPI_NO_CS		(1UL << 6)	/* 1 dev/bus, no chipselect */
#define SPI_READY		(1UL << 7)	/* slave pulls low to pause */
#define SPI_TX_DUAL		(1UL << 8)	/* transmit with 2 wires */
#define SPI_TX_QUAD		(1UL << 9)	/* transmit with 4 wires */
#define SPI_RX_DUAL		(1UL << 10)	/* receive with 2 wires */
#define SPI_RX_QUAD		(1UL << 11)	/* receive with 4 wires */
#define SPI_CS_WORD		(1UL << 12)	/* toggle cs after each word */
#define SPI_TX_OCTAL		(1UL << 13)	/* transmit with 8 wires */
#define SPI_RX_OCTAL		(1UL << 14)	/* receive with 8 wires */
#define SPI_3WIRE_HIZ		(1UL << 15)	/* high impedance turnaround */
#define SPI_RX_CPHA_FLIP	(1UL << 16)	/* flip CPHA on Rx only xfer */
#define SPI_MOSI_IDLE_LOW	(1UL << 17)	/* leave MOSI line low when idle */
#define SPI_MOSI_IDLE_HIGH	(1UL << 18)	/* leave MOSI line high when idle */

/*
 * All bits above are covered by SPI_MODE_USER_MASK.
 * The kernel counterpart uses the opposite end of the bit range, so the
 * two masks must remain disjoint.
 */
#define SPI_MODE_USER_MASK	((1UL << 19) - 1)

#endif /* _UAPI_SPI_H */

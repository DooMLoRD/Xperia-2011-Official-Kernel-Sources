#ifndef _I386_SCATTERLIST_H
#define _I386_SCATTERLIST_H

#include <asm/types.h>

struct scatterlist {
#ifdef CONFIG_DEBUG_SG
    unsigned long	sg_magic;
#endif
    unsigned long	page_link;
    unsigned int	offset;
    dma_addr_t		dma_address;
    unsigned int	length;
};

#define ARCH_HAS_SG_CHAIN

/* These macros should be used after a pci_map_sg call has been done
 * to get bus addresses of each of the SG entries and their lengths.
 * You should only work with the number of sg entries pci_map_sg
 * returns.
 */
#define sg_dma_address(sg)	((sg)->dma_address)
#define sg_dma_len(sg)		((sg)->length)

#define ISA_DMA_THRESHOLD (0x00ffffff)

#endif /* !(_I386_SCATTERLIST_H) */

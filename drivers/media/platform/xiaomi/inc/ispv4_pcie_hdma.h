#ifndef XM_PCI_DMA_H__
#define XM_PCI_DMA_H__

#include "linux/types.h"
#include "media/ispv4_defs.h"

struct pcie_hdma_chan_ctrl;
struct pcie_hdma;

struct pcie_hdma_chan_ctrl *ispv4_hdma_request_chan(struct pcie_hdma *hdma,
						    enum pcie_hdma_dir dir);
void ispv4_hdma_release_chan(struct pcie_hdma_chan_ctrl *chan_ctrl);

int32_t ispv4_hdma_xfer_add_block(struct pcie_hdma_chan_ctrl *chan_ctrl,
				  uint32_t src_addr, uint32_t dst_addr,
				  uint32_t size);

int32_t
ispv4_pcie_hdma_start_and_wait_end(struct pcie_hdma *hdma,
				   struct pcie_hdma_chan_ctrl *chan_ctrl);

#endif

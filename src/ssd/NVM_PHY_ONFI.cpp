#include "NVM_PHY_ONFI.h"

namespace SSD_Components {
	void NVM_PHY_ONFI::ConnectToTransactionServicedSignal(TransactionServicedHandlerType function)  /* 包含mapping GC cache 和 TSU */
	{
		connectedTransactionServicedHandlers.push_back(function);
	}

	/*
	* Different FTL components maybe waiting for a transaction to be finished:
	* HostInterface: For user reads and writes
	* Address_Mapping_Unit: For mapping reads and writes
	* TSU: For the reads that must be finished for partial writes (first read non updated parts of page data and then merge and write them into the new page)
	* GarbageCollector: For gc reads, writes, and erases
	*/
	void NVM_PHY_ONFI::broadcastTransactionServicedSignal(NVM_Transaction_Flash* transaction)  /* 包含mapping GC cache 和 TSU */
	{
		for (std::vector<TransactionServicedHandlerType>::iterator it = connectedTransactionServicedHandlers.begin();
			it != connectedTransactionServicedHandlers.end(); it++) {
			(*it)(transaction);
		}
		delete transaction;//This transaction has been consumed and no more needed
	}

	void NVM_PHY_ONFI::ConnectToChannelIdleSignal(ChannelIdleHandlerType function)  /* 只有TSU */
	{
		connectedChannelIdleHandlers.push_back(function);
	}

	void NVM_PHY_ONFI::broadcastChannelIdleSignal(flash_channel_ID_type channelID)  /* 只有TSU */
	{
		for (std::vector<ChannelIdleHandlerType>::iterator it = connectedChannelIdleHandlers.begin();
			it != connectedChannelIdleHandlers.end(); it++) {
			(*it)(channelID);
		}
	}

	void NVM_PHY_ONFI::ConnectToChipIdleSignal(ChipIdleHandlerType function)  /* 只有TSU */
	{
		connectedChipIdleHandlers.push_back(function);
	}

	void NVM_PHY_ONFI::broadcastChipIdleSignal(NVM::FlashMemory::Flash_Chip* chip)  /* 只有TSU 会被丢到chip层面的广播函数中 chip在finish_command_execution时候结尾调用 */
	{
		for (std::vector<ChipIdleHandlerType>::iterator it = connectedChipIdleHandlers.begin();
			it != connectedChipIdleHandlers.end(); it++) {
			(*it)(chip);
		}
	}
}
#include <string>
#include "TSU_Base.h"

#define TRTOSTR(TR) (TR->Type == Transaction_Type::READ ? "Read, " : (TR->Type == Transaction_Type::WRITE ? "Write, " : "Erase, ") )

namespace SSD_Components
{
	TSU_Base* TSU_Base::_my_instance = NULL;

	TSU_Base::TSU_Base(const sim_object_id_type& id, FTL* ftl, NVM_PHY_ONFI_NVDDR2* NVMController, Flash_Scheduling_Type Type,
		unsigned int ChannelCount, unsigned int chip_no_per_channel, unsigned int DieNoPerChip, unsigned int PlaneNoPerDie,
		bool EraseSuspensionEnabled, bool ProgramSuspensionEnabled,
		sim_time_type WriteReasonableSuspensionTimeForRead,
		sim_time_type EraseReasonableSuspensionTimeForRead,
		sim_time_type EraseReasonableSuspensionTimeForWrite)
		: Sim_Object(id), ftl(ftl), _NVMController(NVMController), type(Type),
		channel_count(ChannelCount), chip_no_per_channel(chip_no_per_channel), die_no_per_chip(DieNoPerChip), plane_no_per_die(PlaneNoPerDie),
		eraseSuspensionEnabled(EraseSuspensionEnabled), programSuspensionEnabled(ProgramSuspensionEnabled),
		writeReasonableSuspensionTimeForRead(WriteReasonableSuspensionTimeForRead), eraseReasonableSuspensionTimeForRead(EraseReasonableSuspensionTimeForRead),
		eraseReasonableSuspensionTimeForWrite(EraseReasonableSuspensionTimeForWrite), opened_scheduling_reqs(0)
	{
		_my_instance = this;
		Round_robin_turn_of_channel = new flash_chip_ID_type[channel_count];
		for (unsigned int channelID = 0; channelID < channel_count; channelID++) {
			Round_robin_turn_of_channel[channelID] = 0;
		}
	}

	TSU_Base::~TSU_Base()
	{
		delete[] Round_robin_turn_of_channel;
	}

	void TSU_Base::Setup_triggers()
	{
		Sim_Object::Setup_triggers();
		_NVMController->ConnectToTransactionServicedSignal(handle_transaction_serviced_signal_from_PHY);
		_NVMController->ConnectToChannelIdleSignal(handle_channel_idle_signal);
		_NVMController->ConnectToChipIdleSignal(handle_chip_idle_signal);
	}

	void TSU_Base::handle_transaction_serviced_signal_from_PHY(NVM_Transaction_Flash* transaction)
	{
		//TSU does nothing. The generator of the transaction will handle it.
	}

	/* 该函数在闪存控制器层面NVDDR2一旦发现有channel空闲就可以调用下面的代码 理解是通知TSU可以开始处理队列中新的事务请求 */
	void TSU_Base::handle_channel_idle_signal(flash_channel_ID_type channelID)  /* 不断轮转查询channelID下的每个chip 如果有可以运行的事务就立马退出 */
	{
		for (unsigned int i = 0; i < _my_instance->chip_no_per_channel; i++) {
			//The TSU does not check if the chip is idle or not since it is possible to suspend a busy chip and issue a new command
			_my_instance->process_chip_requests(_my_instance->_NVMController->Get_chip(channelID, _my_instance->Round_robin_turn_of_channel[channelID]));
			_my_instance->Round_robin_turn_of_channel[channelID] = (flash_chip_ID_type)(_my_instance->Round_robin_turn_of_channel[channelID] + 1) % _my_instance->chip_no_per_channel;

			//A transaction has been started, so TSU should stop searching for another chip
			if (_my_instance->_NVMController->Get_channel_status(channelID) == BusChannelStatus::BUSY) {
				break;
			}
		}
	}
	
	void TSU_Base::handle_chip_idle_signal(NVM::FlashMemory::Flash_Chip* chip)  /* 最终会被丢到chip层面的广播函数中 chip在finish_command_execution时候结尾调用 */
	{
		if (_my_instance->_NVMController->Get_channel_status(chip->ChannelID) == BusChannelStatus::IDLE) {
			_my_instance->process_chip_requests(chip);
		}
	}

	void TSU_Base::Report_results_in_XML(std::string name_prefix, Utils::XmlWriter& xmlwriter)
	{
	}

	bool TSU_Base::issue_command_to_chip(Flash_Transaction_Queue *sourceQueue1, Flash_Transaction_Queue *sourceQueue2, Transaction_Type transactionType, bool suspensionRequired)
	{
		//PRINT_MESSAGE("begin issue_command_to_chip");

		// auto xx = sourceQueue1->front()->Address;
		// if (sourceQueue1 == NULL)
		// 	zz;
		// qq(sourceQueue1->size());
		// qq(sourceQueue1->front()->Address.DieID);

		flash_die_ID_type dieID = sourceQueue1->front()->Address.DieID;
		flash_page_ID_type pageID = sourceQueue1->front()->Address.PageID;
		unsigned int planeVector = 0;
		static int issueCntr = 0;

		for (unsigned int i = 0; i < die_no_per_chip; i++)  /* 分别处理每个die 并在其中每个die上实现内部的多个plane并行 */
		{
			transaction_dispatch_slots.clear();
			planeVector = 0;

			for (Flash_Transaction_Queue::iterator it = sourceQueue1->begin(); it != sourceQueue1->end();)  /* 与下面的for循环一起实现一个die内部multi-plane的并行性 */
			{	/* a << b = a * (2^b)    a >> b = a / (2^b) */
				if (transaction_is_ready(*it) && (*it)->Address.DieID == dieID && !(planeVector & 1 << (*it)->Address.PlaneID))  /* 会先进行<< 目的是防止部分plane在运行时就不能被占用 */
				{
					//Check for identical pages when running multiplane command
					if (planeVector == 0 || (*it)->Address.PageID == pageID)
					{
						(*it)->SuspendRequired = suspensionRequired;
						planeVector |= 1 << (*it)->Address.PlaneID;   /* 将可以占用的plane设置为1 */
						transaction_dispatch_slots.push_back(*it);
						DEBUG(issueCntr++ << ": " << Simulator->Time() <<" Issueing Transaction - Type:" << TRTOSTR((*it)) << ", PPA:" << (*it)->PPA << ", LPA:" << (*it)->LPA << ", Channel: " << (*it)->Address.ChannelID << ", Chip: " << (*it)->Address.ChipID);
						sourceQueue1->remove(it++);
						continue;
					}
				}
				it++;
			}


			if (sourceQueue2 != NULL && transaction_dispatch_slots.size() < plane_no_per_die)   /* 还在处理每个die内部即mult-plane的并行性 */
			{
				for (Flash_Transaction_Queue::iterator it = sourceQueue2->begin(); it != sourceQueue2->end();)
				{
					if (transaction_is_ready(*it) && (*it)->Address.DieID == dieID && !(planeVector & 1 << (*it)->Address.PlaneID))
					{
						//Check for identical pages when running multiplane command
						if (planeVector == 0 || (*it)->Address.PageID == pageID)
						{
							(*it)->SuspendRequired = suspensionRequired;
							planeVector |= 1 << (*it)->Address.PlaneID;
							transaction_dispatch_slots.push_back(*it);
							DEBUG(issueCntr++ << ": " << Simulator->Time() << " Issueing Transaction - Type:" << TRTOSTR((*it)) << ", PPA:" << (*it)->PPA << ", LPA:" << (*it)->LPA << ", Channel: " << (*it)->Address.ChannelID << ", Chip: " << (*it)->Address.ChipID);
							sourceQueue2->remove(it++);
							continue;
						}
					}
					it++;
				}
			}
			
			if (transaction_dispatch_slots.size() != 1 && transaction_dispatch_slots.size() != 2)  /* 感觉只有一个或者两个 注意 你的Plane_No_Per_Die可能就是两个 */
				qq("has more 2 in TSU_base.cpp issue_command_to_chip"<<transaction_dispatch_slots.size());

			if (transaction_dispatch_slots.size() > 0)
			{
				_NVMController->Send_command_to_chip(transaction_dispatch_slots);
				transaction_dispatch_slots.clear();
				dieID = (dieID + 1) % die_no_per_chip;
				return true;
			}
			else
			{
				qq("TSU_Base.cpp -> issue_command_to_chip -> return false");
				transaction_dispatch_slots.clear();
				dieID = (dieID + 1) % die_no_per_chip;
				// return false;    /* 可能是需要被注释的 但是该函数被其他函数调用的时候 返回值好像都没怎么用 返回值也不用一个变量去赋值接收 */
			}			
		}

		return false;
	}
}








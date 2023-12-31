#include "TSU_FLIN.h"
#include "NVM_PHY_ONFI_NVDDR2.h"
#include <stack>
#include <cmath>
#include <typeinfo>


namespace SSD_Components
{

	TSU_FLIN::TSU_FLIN(const sim_object_id_type& id, FTL* ftl, NVM_PHY_ONFI_NVDDR2* NVMController, 
		const unsigned int channel_count, const unsigned int chip_no_per_channel, const unsigned int die_no_per_chip, const unsigned int plane_no_per_die, unsigned int flash_page_size,
		const sim_time_type flow_classification_epoch, const unsigned int alpha_read, const unsigned int alpha_write, /* 10000000, 33554432, 262144 */
		const unsigned int no_of_priority_classes, const stream_id_type max_flow_id, const unsigned int* stream_count_per_priority_class, stream_id_type** stream_ids_per_priority_class, const double f_thr, /* 0.6 */
		const sim_time_type WriteReasonableSuspensionTimeForRead,
		const sim_time_type EraseReasonableSuspensionTimeForRead,
		const sim_time_type EraseReasonableSuspensionTimeForWrite,
		const bool EraseSuspensionEnabled, const bool ProgramSuspensionEnabled)
		: TSU_Base(id, ftl, NVMController, Flash_Scheduling_Type::FLIN, channel_count, chip_no_per_channel, die_no_per_chip, plane_no_per_die,
			WriteReasonableSuspensionTimeForRead, EraseReasonableSuspensionTimeForRead, EraseReasonableSuspensionTimeForWrite,
			EraseSuspensionEnabled, ProgramSuspensionEnabled), 
		    flow_classification_epoch(flow_classification_epoch),  /* 10000000 */
		    no_of_priority_classes(no_of_priority_classes), F_thr(f_thr)
	{
		//PRINT_MESSAGE("begin TSU_FLIN");
		alpha_read_for_epoch = alpha_read / (channel_count * chip_no_per_channel) / flash_page_size;  /* flash_page_size = 8192 */
		alpha_write_for_epoch = alpha_write / (channel_count * chip_no_per_channel) / flash_page_size,
		this->stream_count_per_priority_class = new unsigned int[no_of_priority_classes];
		this->stream_ids_per_priority_class = new stream_id_type*[no_of_priority_classes];
		for (unsigned int i = 0; i < no_of_priority_classes; i++)
		{
			this->stream_count_per_priority_class[i] = stream_count_per_priority_class[i];
			this->stream_ids_per_priority_class[i] = new stream_id_type[stream_count_per_priority_class[i]];
			for (stream_id_type stream_cntr = 0; stream_cntr < stream_count_per_priority_class[i]; stream_cntr++)
				this->stream_ids_per_priority_class[i][stream_cntr] = stream_ids_per_priority_class[i][stream_cntr];
		}
		UserReadTRQueue = new Flash_Transaction_Queue**[channel_count];
		Read_slot = new NVM_Transaction_Flash_RD***[channel_count];
		UserWriteTRQueue = new Flash_Transaction_Queue**[channel_count];
		Write_slot = new NVM_Transaction_Flash_WR***[channel_count];
		GCReadTRQueue = new Flash_Transaction_Queue*[channel_count];
		GCWriteTRQueue = new Flash_Transaction_Queue*[channel_count];
		GCEraseTRQueue = new Flash_Transaction_Queue*[channel_count];
		MappingReadTRQueue = new Flash_Transaction_Queue*[channel_count];
		MappingWriteTRQueue = new Flash_Transaction_Queue*[channel_count];
		flow_activity_info = new FLIN_Flow_Monitoring_Unit***[channel_count];
		low_intensity_class_read = new std::set<stream_id_type>**[channel_count];
		low_intensity_class_write = new std::set<stream_id_type>**[channel_count];

		/* zy1024cs改 */
		head_high_read = new std::list<NVM_Transaction_Flash*>::iterator**[channel_count];
		head_high_write = new std::list<NVM_Transaction_Flash*>::iterator**[channel_count];


		for (unsigned int channel_id = 0; channel_id < channel_count; channel_id++)
		{
			UserReadTRQueue[channel_id] = new Flash_Transaction_Queue*[chip_no_per_channel];
			Read_slot[channel_id] = new NVM_Transaction_Flash_RD**[chip_no_per_channel];
			UserWriteTRQueue[channel_id] = new Flash_Transaction_Queue*[chip_no_per_channel];
			Write_slot[channel_id] = new NVM_Transaction_Flash_WR**[chip_no_per_channel];
			GCReadTRQueue[channel_id] = new Flash_Transaction_Queue[chip_no_per_channel];
			GCWriteTRQueue[channel_id] = new Flash_Transaction_Queue[chip_no_per_channel];
			GCEraseTRQueue[channel_id] = new Flash_Transaction_Queue[chip_no_per_channel];
			MappingReadTRQueue[channel_id] = new Flash_Transaction_Queue[chip_no_per_channel];
			MappingWriteTRQueue[channel_id] = new Flash_Transaction_Queue[chip_no_per_channel];
			flow_activity_info[channel_id] = new FLIN_Flow_Monitoring_Unit**[chip_no_per_channel];
			low_intensity_class_read[channel_id] = new std::set<stream_id_type>*[chip_no_per_channel];
			low_intensity_class_write[channel_id] = new std::set<stream_id_type>*[chip_no_per_channel];

			/* zy1024cs改 */
			head_high_read[channel_id] = new std::list<NVM_Transaction_Flash*>::iterator*[chip_no_per_channel];
			head_high_write[channel_id] = new std::list<NVM_Transaction_Flash*>::iterator*[chip_no_per_channel];
				
			for (unsigned int chip_id = 0; chip_id < chip_no_per_channel; chip_id++)
			{
				UserReadTRQueue[channel_id][chip_id] = new Flash_Transaction_Queue[no_of_priority_classes];
				Read_slot[channel_id][chip_id] = new NVM_Transaction_Flash_RD*[no_of_priority_classes];
				UserWriteTRQueue[channel_id][chip_id] = new Flash_Transaction_Queue[no_of_priority_classes];
				Write_slot[channel_id][chip_id] = new NVM_Transaction_Flash_WR*[no_of_priority_classes];
				flow_activity_info[channel_id][chip_id] = new FLIN_Flow_Monitoring_Unit*[no_of_priority_classes];
				low_intensity_class_read[channel_id][chip_id] = new std::set<stream_id_type>[no_of_priority_classes];
				low_intensity_class_write[channel_id][chip_id] = new std::set<stream_id_type>[no_of_priority_classes];

				/* zy1024cs改 */
				head_high_read[channel_id][chip_id] = new std::list<NVM_Transaction_Flash*>::iterator[no_of_priority_classes];
				head_high_write[channel_id][chip_id] = new std::list<NVM_Transaction_Flash*>::iterator[no_of_priority_classes];

				GCReadTRQueue[channel_id][chip_id].Set_id("GC_Read_TR_Queue@" + std::to_string(channel_id) + "@" + std::to_string(chip_id) + "@");
				MappingReadTRQueue[channel_id][chip_id].Set_id("Mapping_Read_TR_Queue@" + std::to_string(channel_id) + "@" + std::to_string(chip_id));
				MappingWriteTRQueue[channel_id][chip_id].Set_id("Mapping_Write_TR_Queue@" + std::to_string(channel_id) + "@" + std::to_string(chip_id));
				GCWriteTRQueue[channel_id][chip_id].Set_id("GC_Write_TR_Queue@" + std::to_string(channel_id) + "@" + std::to_string(chip_id));
				GCEraseTRQueue[channel_id][chip_id].Set_id("GC_Erase_TR_Queue@" + std::to_string(channel_id) + "@" + std::to_string(chip_id));
				for (unsigned int pclass_id = 0; pclass_id < no_of_priority_classes; pclass_id++)
				{
					flow_activity_info[channel_id][chip_id][pclass_id] = new FLIN_Flow_Monitoring_Unit[max_flow_id];
					for (int stream_cntr = 0; stream_cntr < max_flow_id; stream_cntr++)
					{
						flow_activity_info[channel_id][chip_id][pclass_id][stream_cntr].Serviced_read_requests_history = 0;
						flow_activity_info[channel_id][chip_id][pclass_id][stream_cntr].Serviced_read_requests_recent = 0;
						flow_activity_info[channel_id][chip_id][pclass_id][stream_cntr].Serviced_write_requests_history = 0;
						flow_activity_info[channel_id][chip_id][pclass_id][stream_cntr].Serviced_write_requests_recent = 0;

						flow_activity_info[channel_id][chip_id][pclass_id][stream_cntr].Serviced_read_requests_total = 0;
						flow_activity_info[channel_id][chip_id][pclass_id][stream_cntr].Serviced_write_requests_total = 0;
						flow_activity_info[channel_id][chip_id][pclass_id][stream_cntr].Sum_read_slowdown = 0;
						flow_activity_info[channel_id][chip_id][pclass_id][stream_cntr].Sum_write_slowdown = 0;
					}

					/* zy1024cs改 */
					head_high_read[channel_id][chip_id][pclass_id] = UserReadTRQueue[channel_id][chip_id][pclass_id].begin();
					head_high_write[channel_id][chip_id][pclass_id] = UserWriteTRQueue[channel_id][chip_id][pclass_id].begin();

					UserReadTRQueue[channel_id][chip_id][pclass_id].Set_id("User_Read_TR_Queue@" + std::to_string(channel_id) + "@" + std::to_string(chip_id) + "@" + std::to_string(pclass_id));
					UserWriteTRQueue[channel_id][chip_id][pclass_id].Set_id("User_Write_TR_Queue@" + std::to_string(channel_id) + "@" + std::to_string(chip_id) + "@" + std::to_string(pclass_id));
				}
			}
		}
		initialize_scheduling_turns();
	}

	TSU_FLIN::~TSU_FLIN()
	{
		delete[] this->stream_count_per_priority_class;
		for (unsigned int channel_id = 0; channel_id < channel_count; channel_id++)
		{
			for (unsigned int chip_id = 0; chip_id < chip_no_per_channel; chip_id++)
			{
				delete[] UserReadTRQueue[channel_id][chip_id];
				delete[] Read_slot[channel_id][chip_id];
				delete[] UserWriteTRQueue[channel_id][chip_id];
				delete[] Write_slot[channel_id][chip_id];
				delete[] low_intensity_class_read[channel_id][chip_id];
				delete[] low_intensity_class_write[channel_id][chip_id];
			}
			delete[] UserReadTRQueue[channel_id];
			delete[] Read_slot[channel_id];
			delete[] UserWriteTRQueue[channel_id];
			delete[] Write_slot[channel_id];
			delete[] GCReadTRQueue[channel_id];
			delete[] GCWriteTRQueue[channel_id];
			delete[] GCEraseTRQueue[channel_id];
			delete[] MappingReadTRQueue[channel_id];
			delete[] MappingWriteTRQueue[channel_id];
			delete[] low_intensity_class_read[channel_id];
			delete[] low_intensity_class_write[channel_id];
		}
		delete[] UserReadTRQueue;
		delete[] Read_slot;
		delete[] UserWriteTRQueue;
		delete[] Write_slot;
		delete[] GCReadTRQueue;
		delete[] GCWriteTRQueue;
		delete[] GCEraseTRQueue;
		delete[] MappingReadTRQueue;
		delete[] MappingWriteTRQueue;
		delete[] low_intensity_class_read;
		delete[] low_intensity_class_write;
	}

	void TSU_FLIN::Start_simulation() 
	{
		Simulator->Register_sim_event(flow_classification_epoch, this, 0, 0);
	}

	void TSU_FLIN::Validate_simulation_config() {}

	void TSU_FLIN::Execute_simulator_event(MQSimEngine::Sim_Event* event) 
	{
		// PRINT_MESSAGE("begin Execute_simulator_event");
		//Flow classification as described in Section 5.1 of FLIN paper in ISCA 2018
		for (unsigned int channel_id = 0; channel_id < channel_count; channel_id++)
		{
			for (unsigned int chip_id = 0; chip_id < chip_no_per_channel; chip_id++)
			{
				for (unsigned int pclass_id = 0; pclass_id < no_of_priority_classes; pclass_id++)
				{
					if (stream_count_per_priority_class[pclass_id] < 2){
						return;
					}
					
					/* 将low_intensity_class内的元素全部清除 并根据Serviced_requests_recent和alpha来重新更新 */
					low_intensity_class_read[channel_id][chip_id][pclass_id].clear();
					low_intensity_class_write[channel_id][chip_id][pclass_id].clear();
					for (unsigned int stream_cntr = 0; stream_cntr < stream_count_per_priority_class[pclass_id]; stream_cntr++)
					{
						if (flow_activity_info[channel_id][chip_id][pclass_id][stream_ids_per_priority_class[pclass_id][stream_cntr]].Serviced_read_requests_recent < alpha_read_for_epoch)
							low_intensity_class_read[channel_id][chip_id][pclass_id].insert(stream_ids_per_priority_class[pclass_id][stream_cntr]);
						if (flow_activity_info[channel_id][chip_id][pclass_id][stream_ids_per_priority_class[pclass_id][stream_cntr]].Serviced_write_requests_recent < alpha_write_for_epoch)
							low_intensity_class_write[channel_id][chip_id][pclass_id].insert(stream_ids_per_priority_class[pclass_id][stream_cntr]);
						flow_activity_info[channel_id][chip_id][pclass_id][stream_ids_per_priority_class[pclass_id][stream_cntr]].Serviced_read_requests_history += flow_activity_info[channel_id][chip_id][pclass_id][stream_cntr].Serviced_read_requests_recent;
						flow_activity_info[channel_id][chip_id][pclass_id][stream_ids_per_priority_class[pclass_id][stream_cntr]].Serviced_read_requests_recent = 0;
						flow_activity_info[channel_id][chip_id][pclass_id][stream_ids_per_priority_class[pclass_id][stream_cntr]].Serviced_write_requests_history += flow_activity_info[channel_id][chip_id][pclass_id][stream_cntr].Serviced_write_requests_recent;
						flow_activity_info[channel_id][chip_id][pclass_id][stream_ids_per_priority_class[pclass_id][stream_cntr]].Serviced_write_requests_recent = 0;
					}

					/* zy1024cs改 更新UserTRQueue的head_high */
					auto head_high_read_pos = UserReadTRQueue[channel_id][chip_id][pclass_id].begin();
					while (head_high_read_pos != UserReadTRQueue[channel_id][chip_id][pclass_id].end()) {
						if (low_intensity_class_read[channel_id][chip_id][pclass_id].find((*head_high_read_pos)->Stream_id) == low_intensity_class_read[channel_id][chip_id][pclass_id].end()) {
							break;
						}
						head_high_read_pos++;
					}
					head_high_read[channel_id][chip_id][pclass_id] = head_high_read_pos;

					auto head_high_write_pos = UserWriteTRQueue[channel_id][chip_id][pclass_id].begin();
					while (head_high_write_pos != UserWriteTRQueue[channel_id][chip_id][pclass_id].end()) {
						if (low_intensity_class_write[channel_id][chip_id][pclass_id].find((*head_high_write_pos)->Stream_id) == low_intensity_class_write[channel_id][chip_id][pclass_id].end()) {
							break;
						}
						head_high_write_pos++;
					}
					head_high_write[channel_id][chip_id][pclass_id] = head_high_write_pos;

				}
			}
		}
		
		Simulator->Register_sim_event(Simulator->Time() + flow_classification_epoch, this, 0, 0);
	}

	inline void TSU_FLIN::Prepare_for_transaction_submit()
	{
		opened_scheduling_reqs++;
		if (opened_scheduling_reqs > 1)
			return;
		transaction_receive_slots.clear();
	}

	inline void TSU_FLIN::Submit_transaction(NVM_Transaction_Flash* transaction)
	{
		transaction_receive_slots.push_back(transaction);
	}

	void TSU_FLIN::Schedule()
	{
		// PRINT_MESSAGE("begin Schedule");
		opened_scheduling_reqs--;
		if (opened_scheduling_reqs > 0)
			return;
		if (opened_scheduling_reqs < 0)
			PRINT_ERROR("TSU Schedule function is invoked in an incorrect way!");

		if (transaction_receive_slots.size() == 0)
			return;

		
		for (std::list<NVM_Transaction_Flash*>::iterator it = transaction_receive_slots.begin();
			it != transaction_receive_slots.end(); it++)
		{
			flash_channel_ID_type channel_id = (*it)->Address.ChannelID;
			flash_chip_ID_type chip_id = (*it)->Address.ChipID;
			// PRINT_MESSAGE("begin Schedule");
			// //auto xx = (*it)->UserIORequest->Priority_class;
			// int xx = (*it)->UserIORequest->Priority_class;
			// //PRINT_MESSAGE(typeid((int)(*it)->UserIORequest->Priority_class).name());
			// PRINT_MESSAGE("middle Schedule");
			// //PRINT_MESSAGE(xx);
			// unsigned int obj = xx - 1;
			// std::cout<<"zy"<<std::endl;
			// std::cout<<(*it)->LPA<<std::endl;
			// PRINT_MESSAGE("end Schedule");
			unsigned int priority_class = ((int)(*it)->Priority_class);
			if (priority_class == (unsigned int)10000){
				priority_class = 3;
				// (*it)->Priority_class = IO_Flow_Priority_Class::Priority::LOW;
			}
				
			// qq(priority_class);
			// PRINT_MESSAGE("end Schedule");
			stream_id_type stream_id = (*it)->Stream_id;
			
			switch ((*it)->Type)
			{
			case Transaction_Type::READ:
				switch ((*it)->Source)
				{
				case Transaction_Source_Type::CACHE:
				case Transaction_Source_Type::USERIO:
					if (stream_count_per_priority_class[priority_class] < 2)
					{
						UserReadTRQueue[channel_id][chip_id][priority_class].push_back(*it);
						break;
					}
					flow_activity_info[channel_id][chip_id][priority_class][stream_id].Serviced_read_requests_recent++;
					flow_activity_info[channel_id][chip_id][priority_class][stream_id].Serviced_read_requests_total++;
					if (low_intensity_class_read[channel_id][chip_id][priority_class].find(stream_id) == low_intensity_class_read[channel_id][chip_id][priority_class].end())
					{  /* (*it)->Stream_id属于high_intensity_class */
						UserReadTRQueue[channel_id][chip_id][priority_class].push_back(*it);//Insert(TRnew after Q.Tail)
						auto tail = UserReadTRQueue[channel_id][chip_id][priority_class].end();
						tail--;
						estimate_alone_waiting_time(&UserReadTRQueue[channel_id][chip_id][priority_class], tail);//EstimateAloneWaitingTime(Q, TRnew)
						stream_id_type flow_with_max_average_slowdown;
						double f = fairness_based_on_average_slowdown(channel_id, chip_id, priority_class, true, flow_with_max_average_slowdown);//FairnessBasedOnAverageSlowdown(Q)
						if (f < F_thr && stream_id == flow_with_max_average_slowdown)
							move_forward(&UserReadTRQueue[channel_id][chip_id][priority_class],
								tail,
								head_high_read[channel_id][chip_id][priority_class]);//MoveForward(from Q.Tail up to Q.Taillow + 1)
						else reorder_for_fairness(&UserReadTRQueue[channel_id][chip_id][priority_class],
								head_high_read[channel_id][chip_id][priority_class],  
								--UserReadTRQueue[channel_id][chip_id][priority_class].end());//ReorderForFairness(from Q.Tail to Q.Taillow + 1)
						/* zy1024cs改 reorder_for_fairness(&UserReadTRQueue[channel_id][chip_id][priority_class],
								UserReadTRQueue[channel_id][chip_id][priority_class].begin(),
								head_high_read[channel_id][chip_id][priority_class]); */
					}
					else  /* (*it)->Stream_id属于low_intensity_class */
					{
						UserReadTRQueue[channel_id][chip_id][priority_class].insert(head_high_read[channel_id][chip_id][priority_class], *it);//Insert(TRnew after Q.Taillow)
						auto tail_low = head_high_read[channel_id][chip_id][priority_class];
						tail_low--;
						estimate_alone_waiting_time(&UserReadTRQueue[channel_id][chip_id][priority_class], tail_low);//EstimateAloneWaitingTime(Q, TRnew)
						reorder_for_fairness(&UserReadTRQueue[channel_id][chip_id][priority_class],
							UserReadTRQueue[channel_id][chip_id][priority_class].begin(), tail_low);//ReorderForFairness(from Q.Taillow to Q.Head)
					}
					break;
				case Transaction_Source_Type::MAPPING:
					MappingReadTRQueue[channel_id][chip_id].push_back(*it);
					break;
				case Transaction_Source_Type::GC_WL:
					GCReadTRQueue[channel_id][chip_id].push_back(*it);
					break;
				default:
					PRINT_ERROR("TSU_FLIN: Unhandled source type four read transaction!")
				}
				break;
			case Transaction_Type::WRITE:
				switch ((*it)->Source)
				{
				case Transaction_Source_Type::CACHE:
				case Transaction_Source_Type::USERIO:
					if (stream_count_per_priority_class[priority_class] < 2)
					{
						UserWriteTRQueue[channel_id][chip_id][priority_class].push_back(*it);
						break;
					}
					flow_activity_info[channel_id][chip_id][priority_class][stream_id].Serviced_write_requests_recent++;
					flow_activity_info[channel_id][chip_id][priority_class][stream_id].Serviced_write_requests_total++;
					if (low_intensity_class_write[channel_id][chip_id][priority_class].find(stream_id) == low_intensity_class_write[channel_id][chip_id][priority_class].end())
					{
						UserWriteTRQueue[channel_id][chip_id][priority_class].push_back(*it);//Insert(TRnew after Q.Tail)
						auto tail = UserWriteTRQueue[channel_id][chip_id][priority_class].end();
						tail--;
						estimate_alone_waiting_time(&UserWriteTRQueue[channel_id][chip_id][priority_class], tail);//EstimateAloneWaitingTime(Q, TRnew)
						stream_id_type flow_with_max_average_slowdown;
						double f = fairness_based_on_average_slowdown(channel_id, chip_id, priority_class, false, flow_with_max_average_slowdown);//FairnessBasedOnAverageSlowdown(Q)
						if (f < F_thr && stream_id == flow_with_max_average_slowdown)
							move_forward(&UserWriteTRQueue[channel_id][chip_id][priority_class],
								tail,
								head_high_write[channel_id][chip_id][priority_class]);//MoveForward(from Q.Tail up to Q.Taillow + 1)
						else reorder_for_fairness(&UserWriteTRQueue[channel_id][chip_id][priority_class],
							head_high_write[channel_id][chip_id][priority_class],
							--UserWriteTRQueue[channel_id][chip_id][priority_class].end());//ReorderForFairness(from Q.Tail to Q.Taillow + 1)
						/* zy1024cs改 reorder_for_fairness(&UserWriteTRQueue[channel_id][chip_id][priority_class],
							UserWriteTRQueue[channel_id][chip_id][priority_class].begin(),
							head_high_write[channel_id][chip_id][priority_class]); */
					}
					else
					{
						UserWriteTRQueue[channel_id][chip_id][priority_class].insert(head_high_write[channel_id][chip_id][priority_class], *it);
						auto tail_low = head_high_write[channel_id][chip_id][priority_class];
						tail_low--;
						estimate_alone_waiting_time(&UserWriteTRQueue[channel_id][chip_id][priority_class], tail_low);
						reorder_for_fairness(&UserWriteTRQueue[channel_id][chip_id][priority_class],
							UserWriteTRQueue[channel_id][chip_id][priority_class].begin(), tail_low);
					}
					break;
				case Transaction_Source_Type::MAPPING:
					MappingWriteTRQueue[channel_id][chip_id].push_back(*it);
					break;
				case Transaction_Source_Type::GC_WL:
					GCWriteTRQueue[channel_id][chip_id].push_back(*it);
					break;
				default:
					PRINT_ERROR("TSU_FLIN: Unhandled source type four write transaction!")
				}
				break;
			case Transaction_Type::ERASE:
				GCEraseTRQueue[channel_id][chip_id].push_back(*it);
				break;
			default:
				break;
			}
		}
		
		
		for (flash_channel_ID_type channel_id = 0; channel_id < channel_count; channel_id++)
		{
			if (_NVMController->Get_channel_status(channel_id) == BusChannelStatus::IDLE)
			{
				for (unsigned int i = 0; i < chip_no_per_channel; i++) {
					NVM::FlashMemory::Flash_Chip* chip = _NVMController->Get_chip(channel_id, Round_robin_turn_of_channel[channel_id]);
					//The TSU does not check if the chip is idle or not since it is possible to suspend a busy chip and issue a new command
					if (!service_read_transaction(chip))
						if (!service_write_transaction(chip))
							service_erase_transaction(chip);
					Round_robin_turn_of_channel[channel_id] = (flash_chip_ID_type)(Round_robin_turn_of_channel[channel_id] + 1) % chip_no_per_channel;
					if (_NVMController->Get_channel_status(chip->ChannelID) != BusChannelStatus::IDLE)
						break;
				}
			}
		}
	}

	void TSU_FLIN::reorder_for_fairness(Flash_Transaction_Queue* queue, std::list<NVM_Transaction_Flash*>::iterator start, std::list<NVM_Transaction_Flash*>::iterator end)
	{
		// PRINT_MESSAGE("begin reorder_for_fairness");
		std::list<NVM_Transaction_Flash*>::iterator itr = queue->begin();
		sim_time_type time_to_finish = 0;
		if(_NVMController->Is_chip_busy(*itr))
			if (_NVMController->Expected_finish_time(*itr) > Simulator->Time())
				time_to_finish = _NVMController->Expected_finish_time(*itr) - Simulator->Time();//T^chip_busy

		std::vector<sim_time_type> T_wait_shared_list;
		while (itr != start)
		{
			time_to_finish += _NVMController->Expected_transfer_time(*itr) + _NVMController->Expected_finish_time(*itr);
			itr++;
		}
		//First pass: from start to end to estimate the slowdown of each transaction in its current position
		std::stack<double> min_slowdown_list, max_slowdown_list;
		double slowdown_min = 10000000000000000000.0, slowdown_max = 0;
		while (itr != std::next(end))
		{
			min_slowdown_list.push(slowdown_min);
			max_slowdown_list.push(slowdown_max);

			time_to_finish += _NVMController->Expected_transfer_time(*itr) + _NVMController->Expected_finish_time(*itr);
			sim_time_type T_TR_shared = time_to_finish + (Simulator->Time() - (*itr)->Issue_time);
			T_wait_shared_list.push_back(T_TR_shared);
			double slowdown = (double)T_TR_shared / (double)(*itr)->Estimated_alone_waiting_time;
			if (slowdown < slowdown_min)
				slowdown_min = slowdown;
			if (slowdown > slowdown_max)
				slowdown_max = slowdown;
			itr++;
		}
		double fairness_max = slowdown_min / slowdown_max;

		
		//Second pass: from end to start to find a position for TR_new sitting at end, that maximizes fairness
		sim_time_type T_new_alone = (*end)->Estimated_alone_waiting_time;
		sim_time_type T_new_shared_before = time_to_finish;

		auto final_position = end;
		auto traverser = end;


		time_to_finish -= _NVMController->Expected_transfer_time(*end) + _NVMController->Expected_finish_time(*end);
		double slowdown_min_reverse = 10000000000000000000.0, slowdown_max_reverse = 0;

		

		while (traverser != std::prev(start) && (*traverser)->Stream_id == (*end)->Stream_id)
		{
			sim_time_type T_pos_alone = (*traverser)->Estimated_alone_waiting_time;
			sim_time_type T_pos_shared_before = time_to_finish;
			double slowdown_pos_before = (double)T_pos_shared_before / T_pos_alone;

			sim_time_type T_pos_shared_after = time_to_finish + _NVMController->Expected_transfer_time(*end) + _NVMController->Expected_finish_time(*end);
			double slowdown_pos_after = (double)T_pos_shared_after / T_pos_alone;

			sim_time_type T_new_shared_after = time_to_finish - _NVMController->Expected_transfer_time(*traverser) + _NVMController->Expected_finish_time(*traverser);
			double slowdown_new_after = (double)T_new_shared_after / T_new_alone;

			double slowdown_min = min_slowdown_list.top();
			min_slowdown_list.pop();
			double slowdown_max = max_slowdown_list.top();
			max_slowdown_list.pop();
			if (slowdown_pos_after > slowdown_max)
				slowdown_max = slowdown_pos_after;
			if (slowdown_pos_after < slowdown_min)
				slowdown_min = slowdown_pos_after;

			if (slowdown_pos_after > slowdown_max_reverse)
				slowdown_max_reverse = slowdown_pos_after;
			if (slowdown_pos_after < slowdown_min_reverse)
				slowdown_min_reverse = slowdown_pos_after;
			
			if (slowdown_new_after > slowdown_max_reverse)
				slowdown_max_reverse = slowdown_new_after;
			if (slowdown_new_after < slowdown_min_reverse)
				slowdown_min_reverse = slowdown_new_after;

			double fairness_after = (double)slowdown_min / slowdown_max;
			if (fairness_after > fairness_max)
			{
				fairness_max = fairness_after;
				final_position = traverser;
			}

			if (slowdown_pos_after > slowdown_max_reverse)
				slowdown_max_reverse = slowdown_pos_after;
			if (slowdown_pos_after < slowdown_min_reverse)
				slowdown_min_reverse = slowdown_pos_after;

			time_to_finish -= _NVMController->Expected_transfer_time(*traverser) + _NVMController->Expected_finish_time(*traverser);
			traverser--;
		}

		if (final_position != end)
		{
			NVM_Transaction_Flash* tr = *end;
			queue->remove(end);
			queue->insert(final_position, tr);
		}
	}

	void TSU_FLIN::estimate_alone_waiting_time(Flash_Transaction_Queue* queue, std::list<NVM_Transaction_Flash*>::iterator position)
	{
		// PRINT_MESSAGE("begin estimate_alone_waiting_time");
		auto itr = position;
		// itr++;  /* zy1024cs改 itr++; */
		// qq((*queue).size());
		// if ((*queue).size() == 1)
		// 	return;
		while (itr != queue->begin())
		{
			if ((*itr)->Stream_id == (*position)->Stream_id)
				break;
			itr--;
		}

		if (itr == queue->begin() && (*itr)->Stream_id != (*position)->Stream_id)
		{
			NVM_Transaction_Flash* chip_tr = _NVMController->Is_chip_busy_with_stream(*position);  /* 返回对应事务相同流的正在目标chip上活跃的事务 */
			if (chip_tr == NULL)
				(*position)->Estimated_alone_waiting_time = 0;
			else
			{
				sim_time_type expected_last_time = chip_tr->Issue_time + chip_tr->Estimated_alone_waiting_time
					+ _NVMController->Expected_transfer_time(*position) + _NVMController->Expected_finish_time(*position);

				if (expected_last_time > Simulator->Time())
					(*position)->Estimated_alone_waiting_time = expected_last_time - Simulator->Time();
				
				return;
			}
		}
		
		/* 找到与position相同的flow的最近事务 */
		sim_time_type expected_last_time = (*itr)->Issue_time + (*itr)->Estimated_alone_waiting_time
			+ _NVMController->Expected_transfer_time(*position) + _NVMController->Expected_finish_time(*position);

		if (expected_last_time > Simulator->Time())
			(*position)->Estimated_alone_waiting_time = expected_last_time - Simulator->Time();
	}

	double TSU_FLIN::fairness_based_on_average_slowdown(unsigned int channel_id, unsigned int chip_id, unsigned int priority_class, bool is_read, stream_id_type& flow_with_max_average_slowdown)
	{
		// PRINT_MESSAGE("begin fairness_based_on_average_slowdown");
		double slowdown_max = 0, slowdown_min = 10000000000000000000.0;
		
		if (is_read)
		{
			for (unsigned int i = 0; i < stream_count_per_priority_class[priority_class]; i++)
			{
				double average_slowdown = flow_activity_info[channel_id][chip_id][priority_class][stream_ids_per_priority_class[priority_class][i]].Sum_read_slowdown++;
				if (average_slowdown > slowdown_max)
					slowdown_max = average_slowdown;
				if (average_slowdown < slowdown_min)
					slowdown_min = average_slowdown;
			}
		}
		else
		{
			for (unsigned int i = 0; i < stream_count_per_priority_class[priority_class]; i++)
			{
				double average_slowdown = flow_activity_info[channel_id][chip_id][priority_class][stream_ids_per_priority_class[priority_class][i]].Sum_write_slowdown++;
				if (average_slowdown > slowdown_max)
					slowdown_max = average_slowdown;
				if (average_slowdown < slowdown_min)
					slowdown_min = average_slowdown;
			}
		}
		return (double)slowdown_min / slowdown_max;
	}

	void TSU_FLIN::move_forward(Flash_Transaction_Queue* queue, std::list<NVM_Transaction_Flash*>::iterator TRnew_pos, std::list<NVM_Transaction_Flash*>::iterator ultimate_posistion)
	{
		// PRINT_MESSAGE("begin move_forward");
		auto Tnew_final_pos = TRnew_pos;
		Tnew_final_pos--;
		/* zy1024cs改 while ((*Tnew_final_pos)->Stream_id != (*TRnew_pos)->Stream_id && !(*Tnew_final_pos)->FLIN_Barrier && Tnew_final_pos != ultimate_posistion) */
		while (Tnew_final_pos != ultimate_posistion && (*Tnew_final_pos)->Stream_id != (*TRnew_pos)->Stream_id && !(*Tnew_final_pos)->FLIN_Barrier)
			Tnew_final_pos--;
		if (Tnew_final_pos == ultimate_posistion 
			&& (*Tnew_final_pos)->Stream_id != (*ultimate_posistion)->Stream_id 
			&& !(*TRnew_pos)->FLIN_Barrier)
		{
			NVM_Transaction_Flash* tr = *TRnew_pos;
			queue->remove(TRnew_pos);
			queue->insert(Tnew_final_pos, tr);
			tr->FLIN_Barrier = true;//According to FLIN: When TRnew is moved forward, it is tagged so that no future arriving flash transaction of the high-intensity flows jumps ahead of it
		}
		else
		{
			NVM_Transaction_Flash* tr = *TRnew_pos;
			queue->remove(TRnew_pos);
			Tnew_final_pos--;
			queue->insert(Tnew_final_pos, tr);
			tr->FLIN_Barrier = true;//According to FLIN: When TRnew is moved forward, it is tagged so that no future arriving flash transaction of the high-intensity flows jumps ahead of it
		}
	}

	void TSU_FLIN::initialize_scheduling_turns()
	{
		// PRINT_MESSAGE("begin initialize_scheduling_turns");
		unsigned int total_turns = no_of_priority_classes;
		for (unsigned int i = 0; i < total_turns; i++)
		{
			scheduling_turn_assignments_read.push_back(i);
			scheduling_turn_assignments_write.push_back(i);
		}

		// int k = 0;
		// int priority_class = 1;
	
		// while (priority_class <= int(no_of_priority_classes))
		// {
		// 	unsigned int step = (unsigned int)std::pow(2, no_of_priority_classes);
		// 	for (unsigned int i = (unsigned int)std::pow(2, no_of_priority_classes - 1); i < total_turns; i += step)
		// 	{
		// 		scheduling_turn_assignments_read[i] = priority_class;
		// 		scheduling_turn_assignments_write[i] = priority_class;
		// 	}
		// 	priority_class++;
		// }
		current_turn_read = 0;
		current_turn_write = 0;
	}

	bool TSU_FLIN::service_read_transaction(NVM::FlashMemory::Flash_Chip* chip)
	{
		// PRINT_MESSAGE("begin service_read_transaction");
		Flash_Transaction_Queue *sourceQueue1 = NULL, *sourceQueue2 = NULL;

		int temp_turn = current_turn_read;
		if (MappingReadTRQueue[chip->ChannelID][chip->ChipID].size() > 0)//Flash transactions that are related to FTL mapping data have the highest priority
		{
			//PRINT_MESSAGE("begin service_read_transaction-1");
			sourceQueue1 = &MappingReadTRQueue[chip->ChannelID][chip->ChipID];
			if (ftl->GC_and_WL_Unit->GC_is_in_urgent_mode(chip) && GCReadTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
				sourceQueue2 = &GCReadTRQueue[chip->ChannelID][chip->ChipID];
			else
			{
				// int cntr = 0;
				// while (cntr < scheduling_turn_assignments_read.size())
				// {
				// 	if (UserReadTRQueue[chip->ChannelID][chip->ChipID][scheduling_turn_assignments_read[temp_turn]].size() > 0){
				// 		sourceQueue2 = &UserReadTRQueue[chip->ChannelID][chip->ChipID][scheduling_turn_assignments_read[temp_turn]];
				// 		break;
				// 	}			
				// 	temp_turn++;
				// 	temp_turn %= scheduling_turn_assignments_read.size();
				// }
				for (int cntr = 0; cntr < scheduling_turn_assignments_read.size(); cntr++){
					if (UserReadTRQueue[chip->ChannelID][chip->ChipID][cntr].size() > 0){
						sourceQueue2 = &UserReadTRQueue[chip->ChannelID][chip->ChipID][cntr];
						break;
					}	
				}
			}
		}
		else if (ftl->GC_and_WL_Unit->GC_is_in_urgent_mode(chip))
		{
			//If flash transactions related to GC are prioritzed (non-preemptive execution mode of GC), then GC queues are checked first
			//PRINT_MESSAGE("begin service_read_transaction-2");

			int cntr = 0;
			

			if (GCReadTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
			{
				sourceQueue1 = &GCReadTRQueue[chip->ChannelID][chip->ChipID];
				if (cntr < scheduling_turn_assignments_read.size())
				{
					// while (cntr < scheduling_turn_assignments_read.size())
					// {
					// 	if (UserReadTRQueue[chip->ChannelID][chip->ChipID][scheduling_turn_assignments_read[temp_turn]].size() > 0){
					// 		sourceQueue2 = &UserReadTRQueue[chip->ChannelID][chip->ChipID][scheduling_turn_assignments_read[temp_turn]];
					// 		break;
					// 	}
					// 	temp_turn++;
					// 	temp_turn %= scheduling_turn_assignments_read.size();
					// }
					for (int cntr = 0; cntr < scheduling_turn_assignments_read.size(); cntr++){
						if (UserReadTRQueue[chip->ChannelID][chip->ChipID][cntr].size() > 0){
							sourceQueue2 = &UserReadTRQueue[chip->ChannelID][chip->ChipID][cntr];
							break;
						}	
					}
				}
			}
			else if (GCWriteTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
			{
				return false;
			}
			else if (GCEraseTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
			{
				return false;
			}
			else if (cntr < scheduling_turn_assignments_read.size())
			{
				// while (cntr < scheduling_turn_assignments_read.size())
				// {
				// 	if (UserReadTRQueue[chip->ChannelID][chip->ChipID][scheduling_turn_assignments_read[temp_turn]].size() > 0){
				// 		qq(scheduling_turn_assignments_read[temp_turn]);
				// 		sourceQueue1 = &UserReadTRQueue[chip->ChannelID][chip->ChipID][scheduling_turn_assignments_read[temp_turn]];
				// 		break;
				// 	}
				// 	temp_turn++;
				// 	temp_turn %= scheduling_turn_assignments_read.size();
				// }
				for (int cntr = 0; cntr < scheduling_turn_assignments_read.size(); cntr++){
					if (UserReadTRQueue[chip->ChannelID][chip->ChipID][cntr].size() > 0){
						sourceQueue1 = &UserReadTRQueue[chip->ChannelID][chip->ChipID][cntr];
						break;
					}	
				}
				if (sourceQueue1 == NULL)
					return false;
			}
			else
			{
				return false;
			}
		}
		else
		{
			//If GC is currently executed in the preemptive mode, then user IO transaction queues are checked first
			//PRINT_MESSAGE("begin service_read_transaction-3");
			int cntr = 0;

			if (cntr < scheduling_turn_assignments_read.size())
			{
				// while (cntr < scheduling_turn_assignments_read.size())
				// {
				// 	if (UserReadTRQueue[chip->ChannelID][chip->ChipID][scheduling_turn_assignments_read[temp_turn]].size() > 0){
				// 		sourceQueue1 = &UserReadTRQueue[chip->ChannelID][chip->ChipID][scheduling_turn_assignments_read[temp_turn]];
				// 		break;
				// 	}
				// 	temp_turn++;
				// 	temp_turn %= scheduling_turn_assignments_read.size();
				// }
				for (int cntr = 0; cntr < scheduling_turn_assignments_read.size(); cntr++){
					if (UserReadTRQueue[chip->ChannelID][chip->ChipID][cntr].size() > 0){
						sourceQueue1 = &UserReadTRQueue[chip->ChannelID][chip->ChipID][cntr];
						break;
					}	
				}
				if (sourceQueue1 == NULL)
					return false;
				if (GCReadTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
				{
					sourceQueue2 = &GCReadTRQueue[chip->ChannelID][chip->ChipID];
				}
			}
			else if (GCReadTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
			{
				sourceQueue1 = &GCReadTRQueue[chip->ChannelID][chip->ChipID];
			}
			else
			{
				return false;
			}
		}

		//PRINT_MESSAGE("begin service_read_transaction-left");
		bool suspensionRequired = false;
		ChipStatus cs = _NVMController->GetChipStatus(chip);
		switch (cs)
		{
		case ChipStatus::IDLE:
			break;
		case ChipStatus::WRITING:
			if (!programSuspensionEnabled || _NVMController->HasSuspendedCommand(chip))
			{
				return false;
			}
			if (_NVMController->Expected_finish_time(chip) - Simulator->Time() < writeReasonableSuspensionTimeForRead)
			{
				return false;
			}
			suspensionRequired = true;
		case ChipStatus::ERASING:
			if (!eraseSuspensionEnabled || _NVMController->HasSuspendedCommand(chip))
			{
				return false;
			}
			if (_NVMController->Expected_finish_time(chip) - Simulator->Time() < eraseReasonableSuspensionTimeForRead)
			{
				return false;
			}
			suspensionRequired = true;
		default:
			return false;
		}

		issue_command_to_chip(sourceQueue1, sourceQueue2, Transaction_Type::READ, suspensionRequired);


		return true;
	}

	bool TSU_FLIN::service_write_transaction(NVM::FlashMemory::Flash_Chip* chip)
	{
		// PRINT_MESSAGE("begin service_write_transaction");
		Flash_Transaction_Queue *sourceQueue1 = NULL, *sourceQueue2 = NULL;
		
		int temp_turn = current_turn_write;
		if (ftl->GC_and_WL_Unit->GC_is_in_urgent_mode(chip))
		{
			int cntr = 0;
			if (GCWriteTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
			{
				sourceQueue1 = &GCWriteTRQueue[chip->ChannelID][chip->ChipID];
				
				// while (cntr < scheduling_turn_assignments_write.size())
				// {
				// 	if (UserWriteTRQueue[chip->ChannelID][chip->ChipID][scheduling_turn_assignments_write[temp_turn]].size() > 0){
				// 		sourceQueue2 = &UserWriteTRQueue[chip->ChannelID][chip->ChipID][scheduling_turn_assignments_write[temp_turn]];
				// 		break;
				// 	}
				// 	temp_turn++;
				// 	temp_turn %= scheduling_turn_assignments_read.size();
				// }
				for (int cntr = 0; cntr < scheduling_turn_assignments_write.size(); cntr++){
					if (UserWriteTRQueue[chip->ChannelID][chip->ChipID][cntr].size() > 0){
						sourceQueue2 = &UserWriteTRQueue[chip->ChannelID][chip->ChipID][cntr];
						break;
					}	
				}
			}
			else if (GCEraseTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
			{
				return false;
			}
			else if (cntr < scheduling_turn_assignments_write.size())
			{
				// while (cntr < scheduling_turn_assignments_write.size())
				// {
				// 	if (UserWriteTRQueue[chip->ChannelID][chip->ChipID][scheduling_turn_assignments_write[temp_turn]].size() > 0){
				// 		sourceQueue1 = &UserWriteTRQueue[chip->ChannelID][chip->ChipID][scheduling_turn_assignments_write[temp_turn]];
				// 		break;
				// 	}
				// 	temp_turn++;
				// 	temp_turn %= scheduling_turn_assignments_read.size();
				// }
				for (int cntr = 0; cntr < scheduling_turn_assignments_write.size(); cntr++){
					if (UserWriteTRQueue[chip->ChannelID][chip->ChipID][cntr].size() > 0){
						sourceQueue1 = &UserWriteTRQueue[chip->ChannelID][chip->ChipID][cntr];
						break;
					}	
				}
				if (sourceQueue1 == NULL)
					return false;
			}
			else
			{
				return false;
			}
		}
		else
		{
			//If GC is currently executed in the preemptive mode, then user IO transaction queues are checked first
			int cntr = 0;
			if (cntr < scheduling_turn_assignments_write.size())
			{
				// while (cntr < scheduling_turn_assignments_write.size())
				// {
				// 	if (UserWriteTRQueue[chip->ChannelID][chip->ChipID][scheduling_turn_assignments_write[temp_turn]].size() > 0){
				// 		sourceQueue1 = &UserWriteTRQueue[chip->ChannelID][chip->ChipID][scheduling_turn_assignments_write[temp_turn]];
				// 		break;
				// 	}
				// 	temp_turn++;
				// 	temp_turn %= scheduling_turn_assignments_read.size();
				// }
				for (int cntr = 0; cntr < scheduling_turn_assignments_write.size(); cntr++){
					if (UserWriteTRQueue[chip->ChannelID][chip->ChipID][cntr].size() > 0){
						sourceQueue1 = &UserWriteTRQueue[chip->ChannelID][chip->ChipID][cntr];
						break;
					}	
				}
				if (sourceQueue1 == NULL)
					return false;
				if (GCWriteTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
				{
					sourceQueue2 = &GCWriteTRQueue[chip->ChannelID][chip->ChipID];
				}
			}
			else if (GCWriteTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
			{
				sourceQueue1 = &GCWriteTRQueue[chip->ChannelID][chip->ChipID];
			}
			else
			{
				return false;
			}
		}
		bool suspensionRequired = false;
		ChipStatus cs = _NVMController->GetChipStatus(chip);
		switch (cs)
		{
		case ChipStatus::IDLE:
			break;
		case ChipStatus::ERASING:
			if (!eraseSuspensionEnabled || _NVMController->HasSuspendedCommand(chip))
				return false;
			if (_NVMController->Expected_finish_time(chip) - Simulator->Time() < eraseReasonableSuspensionTimeForWrite)
				return false;
			suspensionRequired = true;
		default:
			return false;
		}

		issue_command_to_chip(sourceQueue1, sourceQueue2, Transaction_Type::WRITE, suspensionRequired);

		return true;
	}

	bool TSU_FLIN::service_erase_transaction(NVM::FlashMemory::Flash_Chip* chip)
	{
		// PRINT_MESSAGE("begin service_erase_transaction");
		if (_NVMController->GetChipStatus(chip) != ChipStatus::IDLE)
		{
			return false;
		}

		Flash_Transaction_Queue *source_queue = &GCEraseTRQueue[chip->ChannelID][chip->ChipID];
		if (source_queue->size() == 0)
		{
			return false;
		}

		issue_command_to_chip(source_queue, NULL, Transaction_Type::ERASE, false);

		return true;
	}

	void TSU_FLIN::Report_results_in_XML(std::string name_prefix, Utils::XmlWriter& xmlwriter)
	{
		name_prefix = name_prefix + ".TSU";
		xmlwriter.Write_open_tag(name_prefix);

		TSU_Base::Report_results_in_XML(name_prefix, xmlwriter);

		for (unsigned int channelID = 0; channelID < channel_count; channelID++)
		{
			for (unsigned int chip_cntr = 0; chip_cntr < chip_no_per_channel; chip_cntr++)
			{
				for (unsigned int priorityClass = 0; priorityClass < IO_Flow_Priority_Class::NUMBER_OF_PRIORITY_LEVELS; priorityClass++)
				{
					UserReadTRQueue[channelID][chip_cntr][priorityClass].Report_results_in_XML(name_prefix + ".User_Read_TR_Queue.Priority." + IO_Flow_Priority_Class::to_string(priorityClass), xmlwriter);
				}
			}
		}

		for (unsigned int channelID = 0; channelID < channel_count; channelID++)
		{
			for (unsigned int chip_cntr = 0; chip_cntr < chip_no_per_channel; chip_cntr++)
			{
				for (unsigned int priorityClass = 0; priorityClass < IO_Flow_Priority_Class::NUMBER_OF_PRIORITY_LEVELS; priorityClass++)
				{
					UserWriteTRQueue[channelID][chip_cntr][priorityClass].Report_results_in_XML(name_prefix + ".User_Write_TR_Queue.Priority." + IO_Flow_Priority_Class::to_string(priorityClass), xmlwriter);
				}
			}
		}

		for (unsigned int channelID = 0; channelID < channel_count; channelID++)
		{
			for (unsigned int chip_cntr = 0; chip_cntr < chip_no_per_channel; chip_cntr++)
			{
				MappingReadTRQueue[channelID][chip_cntr].Report_results_in_XML(name_prefix + ".Mapping_Read_TR_Queue", xmlwriter);
			}
		}

		for (unsigned int channelID = 0; channelID < channel_count; channelID++)
		{
			for (unsigned int chip_cntr = 0; chip_cntr < chip_no_per_channel; chip_cntr++)
			{
				MappingWriteTRQueue[channelID][chip_cntr].Report_results_in_XML(name_prefix + ".Mapping_Write_TR_Queue", xmlwriter);
			}
		}

		for (unsigned int channelID = 0; channelID < channel_count; channelID++)
		{
			for (unsigned int chip_cntr = 0; chip_cntr < chip_no_per_channel; chip_cntr++)
			{
				GCReadTRQueue[channelID][chip_cntr].Report_results_in_XML(name_prefix + ".GC_Read_TR_Queue", xmlwriter);
			}
		}

		for (unsigned int channelID = 0; channelID < channel_count; channelID++)
		{
			for (unsigned int chip_cntr = 0; chip_cntr < chip_no_per_channel; chip_cntr++)
			{
				GCWriteTRQueue[channelID][chip_cntr].Report_results_in_XML(name_prefix + ".GC_Write_TR_Queue", xmlwriter);
			}
		}

		for (unsigned int channelID = 0; channelID < channel_count; channelID++)
		{
			for (unsigned int chip_cntr = 0; chip_cntr < chip_no_per_channel; chip_cntr++)
			{
				GCEraseTRQueue[channelID][chip_cntr].Report_results_in_XML(name_prefix + ".GC_Erase_TR_Queue", xmlwriter);
			}
		}

		xmlwriter.Write_close_tag();
	}

}

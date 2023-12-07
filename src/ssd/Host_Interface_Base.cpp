#include "Host_Interface_Base.h"
#include "Data_Cache_Manager_Base.h"

namespace SSD_Components
{
	Input_Stream_Base::Input_Stream_Base() :
		STAT_number_of_read_requests(0), STAT_number_of_write_requests(0), 
		STAT_number_of_read_transactions(0), STAT_number_of_write_transactions(0),
		STAT_sum_of_read_transactions_execution_time(0), STAT_sum_of_read_transactions_transfer_time(0), STAT_sum_of_read_transactions_waiting_time(0),
		STAT_sum_of_write_transactions_execution_time(0), STAT_sum_of_write_transactions_transfer_time(0), STAT_sum_of_write_transactions_waiting_time(0)
	{}
	
	Input_Stream_Manager_Base::~Input_Stream_Manager_Base()
	{
		for (auto &stream : input_streams) {
			delete stream;
		}
	}

	Input_Stream_Base::~Input_Stream_Base()
	{
	}

	Request_Fetch_Unit_Base::~Request_Fetch_Unit_Base()
	{
		for (auto &dma_info : dma_list) {
			delete dma_info;
		}
	}

	Host_Interface_Base* Host_Interface_Base::_my_instance = NULL;

	Host_Interface_Base::Host_Interface_Base(const sim_object_id_type& id, HostInterface_Types type, LHA_type max_logical_sector_address, unsigned int sectors_per_page, 
		Data_Cache_Manager_Base* cache)
		: MQSimEngine::Sim_Object(id), type(type), max_logical_sector_address(max_logical_sector_address), 
		sectors_per_page(sectors_per_page), cache(cache)
	{
		_my_instance = this;
	}
	
	Host_Interface_Base::~Host_Interface_Base()
	{
		delete input_stream_manager;
		delete request_fetch_unit;
	}

	void Host_Interface_Base::Setup_triggers()
	{
		Sim_Object::Setup_triggers();
		cache->Connect_to_user_request_serviced_signal(handle_user_request_serviced_signal_from_cache);  /* connected_user_request_serviced_signal_handlers.push_back(function); */
		cache->Connect_to_user_memory_transaction_serviced_signal(handle_user_memory_transaction_serviced_signal_from_cache);  /* PHY告知host对缓存中的每个完成的事务进行相关时间统计 connected_user_memory_transaction_serviced_signal_handlers.push_back(function); */
	}

	void Host_Interface_Base::Validate_simulation_config()
	{
	}

	void Host_Interface_Base::Send_read_message_to_host(uint64_t addresss, unsigned int request_read_data_size)
	{   /* 从主机端发起读一个提交实体 被src/ssd/Host_Interface_NVMe.cpp中的Fetch_next_request和Fetch_write_data调用 */
		Host_Components::PCIe_Message* pcie_message = new Host_Components::PCIe_Message;
		pcie_message->Type = Host_Components::PCIe_Message_Type::READ_REQ;
		pcie_message->Destination = Host_Components::PCIe_Destination_Type::HOST;
		pcie_message->Address = addresss;
		pcie_message->Payload = (void*)(intptr_t)request_read_data_size;
		pcie_message->Payload_size = sizeof(request_read_data_size);
		pcie_switch->Send_to_host(pcie_message);
	}

	void Host_Interface_Base::Send_write_message_to_host(uint64_t addresss, void* message, unsigned int message_size)
	{	/* 向主机端内存的完成队列中写一个完成实体 被src/ssd/Host_Interface_NVMe.cpp中的Send_completion_queue_element和Send_read_data调用 */
		Host_Components::PCIe_Message* pcie_message = new Host_Components::PCIe_Message;
		pcie_message->Type = Host_Components::PCIe_Message_Type::WRITE_REQ;
		pcie_message->Destination = Host_Components::PCIe_Destination_Type::HOST;
		pcie_message->Address = addresss;
		COPYDATA(pcie_message->Payload, message, pcie_message->Payload_size);  /* COPYDATA(DEST,SRC,SIZE) */
		pcie_message->Payload_size = message_size;
		pcie_switch->Send_to_host(pcie_message);
	}

	void Host_Interface_Base::Attach_to_device(Host_Components::PCIe_Switch* pcie_switch)
	{
		this->pcie_switch = pcie_switch;
	}

	LHA_type Host_Interface_Base::Get_max_logical_sector_address()
	{
		return max_logical_sector_address;
	}

	unsigned int Host_Interface_Base::Get_no_of_LHAs_in_an_NVM_write_unit()
	{
		return sectors_per_page;
	}

	Input_Stream_Manager_Base::Input_Stream_Manager_Base(Host_Interface_Base* host_interface) :
		host_interface(host_interface)
	{
	}

	void Input_Stream_Manager_Base::Update_transaction_statistics(NVM_Transaction* transaction)
	{	/* 通过广播的方式在cache中被调用 */

		// std::string obj = transaction->Info_of_turn();
		// obj = obj + " STAT_execution_time: " + std::to_string(transaction->STAT_execution_time) + " STAT_transfer_time: " + std::to_string(transaction->STAT_transfer_time);
		// obj = obj + " transactions_waiting_time: " + std::to_string((Simulator->Time() - transaction->Issue_time) - transaction->STAT_execution_time - transaction->STAT_transfer_time);
		// obj = obj + " tatal_time: " + std::to_string(Simulator->Time() - transaction->Issue_time);
		// qq(obj);
		// qq((Simulator->Time() - transaction->Issue_time) - transaction->STAT_execution_time - transaction->STAT_transfer_time - (transaction->tsu_time - transaction->mapping_time) - (transaction->flash_time - transaction->tsu_time));
		// if (transaction->cache_time - transaction->Issue_time != 0)
		// 	qq(transaction->cache_time - transaction->Issue_time);


		/* zy1024cs */
		if (transaction->cache_time != -1 && transaction->tsu_time != -1 && transaction->flash_time != -1) {
			switch (transaction->Type)
			{
				case Transaction_Type::READ:
					this->input_streams[transaction->Stream_id]->STAT_number_of_read_transactions_via_flash++;
					this->input_streams[transaction->Stream_id]->STAT_sum_of_read_transactions_cache_time += (transaction->tsu_time - transaction->cache_time);
					this->input_streams[transaction->Stream_id]->STAT_sum_of_read_transactions_tsu_time += (transaction->flash_time - transaction->tsu_time);
					this->input_streams[transaction->Stream_id]->STAT_sum_of_read_transactions_all_time += (Simulator->Time() - transaction->Issue_time);
					break;
				case Transaction_Type::WRITE:
					this->input_streams[transaction->Stream_id]->STAT_number_of_write_transactions_via_flash++;
					this->input_streams[transaction->Stream_id]->STAT_sum_of_write_transactions_cache_time += (transaction->tsu_time - transaction->cache_time);
					this->input_streams[transaction->Stream_id]->STAT_sum_of_write_transactions_tsu_time += (transaction->flash_time - transaction->tsu_time);
					this->input_streams[transaction->Stream_id]->STAT_sum_of_write_transactions_all_time += (Simulator->Time() - transaction->Issue_time);
					break;
				default:
					break;
			}
		}





		switch (transaction->Type)
		{
			case Transaction_Type::READ:
				this->input_streams[transaction->Stream_id]->STAT_sum_of_read_transactions_execution_time += transaction->STAT_execution_time;
				this->input_streams[transaction->Stream_id]->STAT_sum_of_read_transactions_transfer_time += transaction->STAT_transfer_time;
				this->input_streams[transaction->Stream_id]->STAT_sum_of_read_transactions_waiting_time += (Simulator->Time() - transaction->Issue_time) - transaction->STAT_execution_time - transaction->STAT_transfer_time;
				break;
			case Transaction_Type::WRITE:
				this->input_streams[transaction->Stream_id]->STAT_sum_of_write_transactions_execution_time += transaction->STAT_execution_time;
				this->input_streams[transaction->Stream_id]->STAT_sum_of_write_transactions_transfer_time += transaction->STAT_transfer_time;
				this->input_streams[transaction->Stream_id]->STAT_sum_of_write_transactions_waiting_time += (Simulator->Time() - transaction->Issue_time) - transaction->STAT_execution_time - transaction->STAT_transfer_time;
				break;
			default:
				break;
		}
	}

	uint32_t Input_Stream_Manager_Base::Get_average_read_transaction_turnaround_time(stream_id_type stream_id)//in microseconds
	{
		if (input_streams[stream_id]->STAT_number_of_read_transactions == 0) {
			return 0;
		}
		return (uint32_t)((input_streams[stream_id]->STAT_sum_of_read_transactions_execution_time + input_streams[stream_id]->STAT_sum_of_read_transactions_transfer_time + input_streams[stream_id]->STAT_sum_of_read_transactions_waiting_time)
			/ input_streams[stream_id]->STAT_number_of_read_transactions / SIM_TIME_TO_MICROSECONDS_COEFF);
	}

	uint32_t Input_Stream_Manager_Base::Get_average_read_transaction_execution_time(stream_id_type stream_id)//in microseconds
	{
		if (input_streams[stream_id]->STAT_number_of_read_transactions == 0) {
			return 0;
		}
		return (uint32_t)(input_streams[stream_id]->STAT_sum_of_read_transactions_execution_time / input_streams[stream_id]->STAT_number_of_read_transactions / SIM_TIME_TO_MICROSECONDS_COEFF);
	}

	uint32_t Input_Stream_Manager_Base::Get_average_read_transaction_transfer_time(stream_id_type stream_id)//in microseconds
	{
		if (input_streams[stream_id]->STAT_number_of_read_transactions == 0) {
			return 0;
		}
		return (uint32_t)(input_streams[stream_id]->STAT_sum_of_read_transactions_transfer_time / input_streams[stream_id]->STAT_number_of_read_transactions / SIM_TIME_TO_MICROSECONDS_COEFF);
	}

	uint32_t Input_Stream_Manager_Base::Get_average_read_transaction_waiting_time(stream_id_type stream_id)//in microseconds
	{
		if (input_streams[stream_id]->STAT_number_of_read_transactions == 0) {
			return 0;
		}
		return (uint32_t)(input_streams[stream_id]->STAT_sum_of_read_transactions_waiting_time / input_streams[stream_id]->STAT_number_of_read_transactions / SIM_TIME_TO_MICROSECONDS_COEFF);
	}

	uint32_t Input_Stream_Manager_Base::Get_average_write_transaction_turnaround_time(stream_id_type stream_id)//in microseconds
	{
		if (input_streams[stream_id]->STAT_number_of_write_transactions == 0) {
			return 0;
		}
		return (uint32_t)((input_streams[stream_id]->STAT_sum_of_write_transactions_execution_time + input_streams[stream_id]->STAT_sum_of_write_transactions_transfer_time + input_streams[stream_id]->STAT_sum_of_write_transactions_waiting_time)
			/ input_streams[stream_id]->STAT_number_of_write_transactions / SIM_TIME_TO_MICROSECONDS_COEFF);
	}

	uint32_t Input_Stream_Manager_Base::Get_average_write_transaction_execution_time(stream_id_type stream_id)//in microseconds
	{
		if (input_streams[stream_id]->STAT_number_of_write_transactions == 0) {
			return 0;
		}
		return (uint32_t)(input_streams[stream_id]->STAT_sum_of_write_transactions_execution_time / input_streams[stream_id]->STAT_number_of_write_transactions / SIM_TIME_TO_MICROSECONDS_COEFF);
	}

	uint32_t Input_Stream_Manager_Base::Get_average_write_transaction_transfer_time(stream_id_type stream_id)//in microseconds
	{
		if (input_streams[stream_id]->STAT_number_of_write_transactions == 0) {
			return 0;
		}
		return (uint32_t)(input_streams[stream_id]->STAT_sum_of_write_transactions_transfer_time / input_streams[stream_id]->STAT_number_of_write_transactions / SIM_TIME_TO_MICROSECONDS_COEFF);
	}

	uint32_t Input_Stream_Manager_Base::Get_average_write_transaction_waiting_time(stream_id_type stream_id)//in microseconds
	{
		if (input_streams[stream_id]->STAT_number_of_write_transactions == 0) {
			return 0;
		}
		return (uint32_t)(input_streams[stream_id]->STAT_sum_of_write_transactions_waiting_time / input_streams[stream_id]->STAT_number_of_write_transactions / SIM_TIME_TO_MICROSECONDS_COEFF);
	}
	
	Request_Fetch_Unit_Base::Request_Fetch_Unit_Base(Host_Interface_Base* host_interface) :
		host_interface(host_interface)
	{
	}


	/* zy1024cs */

	uint32_t Input_Stream_Manager_Base::Get_average_read_transaction_cache_time(stream_id_type stream_id) {
		if (input_streams[stream_id]->STAT_number_of_read_transactions_via_flash == 0) {
			return 0;
		}
		return (uint32_t)(input_streams[stream_id]->STAT_sum_of_read_transactions_cache_time / input_streams[stream_id]->STAT_number_of_read_transactions_via_flash / SIM_TIME_TO_MICROSECONDS_COEFF);
	}
	uint32_t Input_Stream_Manager_Base::Get_average_read_transaction_tsu_time(stream_id_type stream_id) {
		if (input_streams[stream_id]->STAT_number_of_read_transactions_via_flash == 0) {
			return 0;
		}
		return (uint32_t)(input_streams[stream_id]->STAT_sum_of_read_transactions_tsu_time / input_streams[stream_id]->STAT_number_of_read_transactions_via_flash / SIM_TIME_TO_MICROSECONDS_COEFF);
	}
	uint32_t Input_Stream_Manager_Base::Get_average_read_transaction_all_time(stream_id_type stream_id) {
		if (input_streams[stream_id]->STAT_number_of_read_transactions_via_flash == 0) {
			return 0;
		}
		return (uint32_t)(input_streams[stream_id]->STAT_sum_of_read_transactions_all_time / input_streams[stream_id]->STAT_number_of_read_transactions_via_flash / SIM_TIME_TO_MICROSECONDS_COEFF);
	}
	uint32_t Input_Stream_Manager_Base::Get_average_write_transaction_cache_time(stream_id_type stream_id) {
		if (input_streams[stream_id]->STAT_number_of_write_transactions_via_flash == 0) {
			return 0;
		}
		return (uint32_t)(input_streams[stream_id]->STAT_sum_of_write_transactions_cache_time / input_streams[stream_id]->STAT_number_of_write_transactions_via_flash / SIM_TIME_TO_MICROSECONDS_COEFF);
	}
	uint32_t Input_Stream_Manager_Base::Get_average_write_transaction_tsu_time(stream_id_type stream_id) {
		if (input_streams[stream_id]->STAT_number_of_write_transactions_via_flash == 0) {
			return 0;
		}
		return (uint32_t)(input_streams[stream_id]->STAT_sum_of_write_transactions_tsu_time / input_streams[stream_id]->STAT_number_of_write_transactions_via_flash / SIM_TIME_TO_MICROSECONDS_COEFF);
	}
	uint32_t Input_Stream_Manager_Base::Get_average_write_transaction_all_time(stream_id_type stream_id) {
		if (input_streams[stream_id]->STAT_number_of_write_transactions_via_flash == 0) {
			return 0;
		}
		return (uint32_t)(input_streams[stream_id]->STAT_sum_of_write_transactions_all_time / input_streams[stream_id]->STAT_number_of_write_transactions_via_flash / SIM_TIME_TO_MICROSECONDS_COEFF);
	}
	std::string Input_Stream_Manager_Base::Get_statistics_string(stream_id_type stream_id) {
		std::string Rs, Ws;
		uint32_t rc, rt, ra, wc, wt, wa;
		rc = Get_average_read_transaction_cache_time(stream_id);
		rt = Get_average_read_transaction_tsu_time(stream_id);
		ra = Get_average_read_transaction_all_time(stream_id);
		wc = Get_average_write_transaction_cache_time(stream_id);
		wt = Get_average_write_transaction_tsu_time(stream_id);
		wa = Get_average_write_transaction_all_time(stream_id);
		if (ra != 0) {
			Rs = std::to_string(rc * 1.0 / ra) + "," + std::to_string(rt * 1.0 / ra);
		}
		else {
			Rs = "none,none";
		}
		if (wa != 0) {
			Ws = std::to_string(wc * 1.0 / wa) + "," + std::to_string(wt * 1.0 / wa);
		}
		else {
			Ws = "none,none";
		}
		return Rs + "," + Ws;
	}


	std::string Input_Stream_Manager_Base::Get_transaction_statistics_string(stream_id_type stream_id) {
		std::string Rs, Ws;
		uint32_t rw, re, rt, ra, ww, we, wt, wa;
		rw = Get_average_read_transaction_waiting_time(stream_id);
		re = Get_average_read_transaction_execution_time(stream_id);
		rt = Get_average_read_transaction_transfer_time(stream_id);
		ra = Get_average_read_transaction_turnaround_time(stream_id);
		ww = Get_average_write_transaction_waiting_time(stream_id);
		we = Get_average_write_transaction_execution_time(stream_id);
		wt = Get_average_write_transaction_transfer_time(stream_id);
		wa = Get_average_write_transaction_turnaround_time(stream_id);
		if (ra != 0) {
			Rs = std::to_string(rw * 1.0 / ra) + "," + std::to_string(re * 1.0 / ra) + "," + std::to_string(rt * 1.0 / ra);
		}
		else {
			Rs = "none,none,none";
		}
		if (wa != 0) {
			Ws = std::to_string(ww * 1.0 / wa) + "," + std::to_string(we * 1.0 / wa) + "," + std::to_string(wt * 1.0 / wa);
		}
		else {
			Ws = "none,none,none";
		}
		return Rs + "," + Ws;
	}



	double Input_Stream_Manager_Base::Get_bandwidth(stream_id_type stream_id) {
		double sum_bandwidth = 0.0;
		for (std::list<int>::iterator it = bandwidth[stream_id]->begin(); it != bandwidth[stream_id]->end(); it++) {
			sum_bandwidth += (*it);
		}
		return sum_bandwidth / 2;  /* KiB/reset_step */
	}

	void Input_Stream_Manager_Base::Calculate_and_update(stream_id_type stream_id, User_Request *request) {
		bool flag = true;

		// if (next_reset_time == -1) {
		next_reset_time = Simulator->Time();
		// }

		// if (Simulator->Time() >= next_reset_time) {
		// 	while (Simulator->Time() >= next_reset_time) {
				last_reset_time = next_reset_time - reset_step;
				// for (int i = 0; i < 60; i++)
				// 	next_reset_time += reset_step;
			// }
			flag = true;
		// }
		

		if (flag) {
			double bw = band_arr[stream_id] * 1.0 / 2;
			double rar = (band_arr[stream_id] != 0) ? (rar_arr[stream_id] * 1.0 / band_arr[stream_id]) : 0.0;
			// double rar = (band_arr[stream_id] != 0) ? (rar_set[stream_id]->size() * 1.0 / band_arr[stream_id]) : 0.0;
			// qq(rar_set[stream_id]->size() << " " << band_arr[stream_id]);
			// qq(bw << " " << rar);
			request->band_rar[0] = bw, request->band_rar[1] = rar;

			double bw_W = band_arr_W[stream_id] * 1.0 / 2;
			double rar_W = (band_arr_W[stream_id] != 0) ? (rar_arr_W[stream_id] * 1.0 / band_arr_W[stream_id]) : 0.0;
			request->band_rar[2] = bw_W, request->band_rar[3] = rar_W;

			// wait(0.3);
		}


		if (request->Type == UserRequestType::READ) {
			while (all_node[stream_id]->size() > 0 && (all_node[stream_id]->back()).Arrival_time < last_reset_time) {
				Node node = all_node[stream_id]->back();
				
				for (LHA_type shift = 0; shift < node.SizeInSectors; shift++) {
					// std::map<LHA_type, sim_time_type>::iterator it = rar_time[stream_id]->find(node.Start_LBA + shift);
					// if (it == rar_time[stream_id]->end()) {

					// }
					// else if ((*it).second < last_reset_time) {
					// 	rar_set[stream_id]->erase((*it).first);
					// 	rar_time[stream_id]->erase(it);
					// }
					// else {
						
					// }

					std::map<LHA_type, int>::iterator it_frq = rar_frq[stream_id]->find(node.Start_LBA + shift);
					if (it_frq == rar_frq[stream_id]->end()) {

					}
					else {
						if ((*it_frq).second >= 2) {
							(*it_frq).second -= 1;
							rar_arr[stream_id] -= 1;
						}
						else {
							rar_frq[stream_id]->erase(it_frq);
						}
					}
				}
				
				band_arr[stream_id] -= node.SizeInSectors;
				all_node[stream_id]->pop_back();
			}

			all_node[stream_id]->push_front(Node(request->Start_LBA, request->SizeInSectors, Simulator->Time()));
			for (LHA_type shift = 0; shift < request->SizeInSectors; shift++) {
				// rar_set[stream_id]->insert(request->Start_LBA + shift);	
				// std::map<LHA_type, sim_time_type>::iterator it = rar_time[stream_id]->find(request->Start_LBA + shift);
				// if (it == rar_time[stream_id]->end()) {
				// 	rar_time[stream_id]->insert(std::map<LHA_type, sim_time_type>::value_type(request->Start_LBA + shift, Simulator->Time()));
				// }
				// else {
				// 	(*it).second = Simulator->Time();
				// }

				std::map<LHA_type, int>::iterator it_frq = rar_frq[stream_id]->find(request->Start_LBA + shift);
				if (it_frq == rar_frq[stream_id]->end()) {
					rar_frq[stream_id]->insert(std::map<LHA_type, int>::value_type(request->Start_LBA + shift, 1));
				}
				else {
					(*it_frq).second += 1;
					rar_arr[stream_id] += 1;
				}
			}
			band_arr[stream_id] += request->SizeInSectors;
		}
		else {   /* request->Type == UserRequestType::WRITE */
			while (all_node_W[stream_id]->size() > 0 && (all_node_W[stream_id]->back()).Arrival_time < last_reset_time) {
				Node node = all_node_W[stream_id]->back();
				
				for (LHA_type shift = 0; shift < node.SizeInSectors; shift++) {
					// std::map<LHA_type, sim_time_type>::iterator it = rar_time_W[stream_id]->find(node.Start_LBA + shift);
					// if (it == rar_time_W[stream_id]->end()) {

					// }
					// else if ((*it).second < last_reset_time) {
					// 	rar_set_W[stream_id]->erase((*it).first);
					// 	rar_time_W[stream_id]->erase(it);
					// }
					// else {
						
					// }

					std::map<LHA_type, int>::iterator it_frq = rar_frq_W[stream_id]->find(node.Start_LBA + shift);
					if (it_frq == rar_frq_W[stream_id]->end()) {

					}
					else {
						if ((*it_frq).second >= 2) {
							(*it_frq).second -= 1;
							rar_arr_W[stream_id] -= 1;
						}
						else {
							rar_frq_W[stream_id]->erase(it_frq);
						}
					}
				}
				
				band_arr_W[stream_id] -= node.SizeInSectors;
				all_node_W[stream_id]->pop_back();
			}

			all_node_W[stream_id]->push_front(Node(request->Start_LBA, request->SizeInSectors, Simulator->Time()));
			for (LHA_type shift = 0; shift < request->SizeInSectors; shift++) {
				// rar_set_W[stream_id]->insert(request->Start_LBA + shift);	
				// std::map<LHA_type, sim_time_type>::iterator it = rar_time_W[stream_id]->find(request->Start_LBA + shift);
				// if (it == rar_time_W[stream_id]->end()) {
				// 	rar_time_W[stream_id]->insert(std::map<LHA_type, sim_time_type>::value_type(request->Start_LBA + shift, Simulator->Time()));
				// }
				// else {
				// 	(*it).second = Simulator->Time();
				// }

				std::map<LHA_type, int>::iterator it_frq = rar_frq_W[stream_id]->find(request->Start_LBA + shift);
				if (it_frq == rar_frq_W[stream_id]->end()) {
					rar_frq_W[stream_id]->insert(std::map<LHA_type, int>::value_type(request->Start_LBA + shift, 1));
				}
				else {
					(*it_frq).second += 1;
					rar_arr_W[stream_id] += 1;
				}
			}
			band_arr_W[stream_id] += request->SizeInSectors;
		}
		

		
		
	}


}
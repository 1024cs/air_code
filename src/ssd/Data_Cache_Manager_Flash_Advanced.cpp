#include <stdexcept>
#include "../nvm_chip/NVM_Types.h"
#include "Data_Cache_Manager_Flash_Advanced.h"
#include "NVM_Transaction_Flash_RD.h"
#include "NVM_Transaction_Flash_WR.h"
#include "FTL.h"
#include "TSU_ZY.h"

namespace SSD_Components
{
	Data_Cache_Manager_Flash_Advanced::Data_Cache_Manager_Flash_Advanced(const sim_object_id_type& id, Host_Interface_Base* host_interface, NVM_Firmware* firmware, NVM_PHY_ONFI* flash_controller,
		unsigned int total_capacity_in_bytes,
		unsigned int dram_row_size, unsigned int dram_data_rate, unsigned int dram_busrt_size, sim_time_type dram_tRCD, sim_time_type dram_tCL, sim_time_type dram_tRP,
		Caching_Mode* caching_mode_per_input_stream, Cache_Sharing_Mode sharing_mode, unsigned int stream_count,
		unsigned int sector_no_per_page, unsigned int back_pressure_buffer_max_depth)
		: Data_Cache_Manager_Base(id, host_interface, firmware, dram_row_size, dram_data_rate, dram_busrt_size, dram_tRCD, dram_tCL, dram_tRP, caching_mode_per_input_stream, sharing_mode, stream_count),
		flash_controller(flash_controller), capacity_in_bytes(total_capacity_in_bytes), sector_no_per_page(sector_no_per_page),	memory_channel_is_busy(false),
		dram_execution_list_turn(0), back_pressure_buffer_max_depth(back_pressure_buffer_max_depth)
	{
		capacity_in_pages = capacity_in_bytes / (SECTOR_SIZE_IN_BYTE * sector_no_per_page);
		// qq(capacity_in_pages<<"-"<<sector_no_per_page);
		switch (sharing_mode)
		{
			case SSD_Components::Cache_Sharing_Mode::SHARED:
			{
				Data_Cache_Flash* sharedCache = new Data_Cache_Flash(capacity_in_pages);
				per_stream_cache = new Data_Cache_Flash*[stream_count];
				for (unsigned int i = 0; i < stream_count; i++) {
					per_stream_cache[i] = sharedCache;
				}
				dram_execution_queue = new std::queue<Memory_Transfer_Info*>[1];
				waiting_user_requests_queue_for_dram_free_slot = new std::list<User_Request*>[1];
				this->back_pressure_buffer_depth = new unsigned int[1];
				this->back_pressure_buffer_depth[0] = 0;
				shared_dram_request_queue = true;
				break;
			}
			case SSD_Components::Cache_Sharing_Mode::EQUAL_PARTITIONING:
				per_stream_cache = new Data_Cache_Flash*[stream_count];
				for (unsigned int i = 0; i < stream_count; i++) {
					per_stream_cache[i] = new Data_Cache_Flash(capacity_in_pages / stream_count);
				}
				dram_execution_queue = new std::queue<Memory_Transfer_Info*>[stream_count];
				waiting_user_requests_queue_for_dram_free_slot = new std::list<User_Request*>[stream_count];
				this->back_pressure_buffer_depth = new unsigned int[stream_count];
				for (unsigned int i = 0; i < stream_count; i++) {
					this->back_pressure_buffer_depth[i] = 0;
				}
				shared_dram_request_queue = false;
				break;
			default:
				break;
		}

		bloom_filter = new std::set<LPA_type>[stream_count];
	}
	
	Data_Cache_Manager_Flash_Advanced::~Data_Cache_Manager_Flash_Advanced()
	{
		switch (sharing_mode)
		{
			case SSD_Components::Cache_Sharing_Mode::SHARED:
			{
				delete per_stream_cache[0];
				while (dram_execution_queue[0].size()) {
					delete dram_execution_queue[0].front();
					dram_execution_queue[0].pop();
				}
				for (auto &req : waiting_user_requests_queue_for_dram_free_slot[0]) {
					delete req;
				}
				break;
			}
			case SSD_Components::Cache_Sharing_Mode::EQUAL_PARTITIONING:
				for (unsigned int i = 0; i < stream_count; i++) {
					delete per_stream_cache[i];
					while (dram_execution_queue[i].size()) {
						delete dram_execution_queue[i].front();
						dram_execution_queue[i].pop();
					}
					for (auto &req : waiting_user_requests_queue_for_dram_free_slot[i]) {
						delete req;
					}
				}
				break;
			default:
				break;
		}
		
		delete[] per_stream_cache;
		delete[] dram_execution_queue;
		delete[] waiting_user_requests_queue_for_dram_free_slot;
		delete[] bloom_filter;
	}

	void Data_Cache_Manager_Flash_Advanced::Setup_triggers()
	{
		Data_Cache_Manager_Base::Setup_triggers();
		flash_controller->ConnectToTransactionServicedSignal(handle_transaction_serviced_signal_from_PHY);
	}

	void Data_Cache_Manager_Flash_Advanced::Do_warmup(std::vector<Utils::Workload_Statistics*> workload_stats)
	{
		double total_write_arrival_rate = 0, total_read_arrival_rate = 0;
		switch (sharing_mode) {
			case Cache_Sharing_Mode::SHARED:
				//Estimate read arrival and write arrival rate
				//Estimate the queue length based on the arrival rate
				for (auto &stat : workload_stats) {
					switch (caching_mode_per_input_stream[stat->Stream_id]) {
						case Caching_Mode::TURNED_OFF:
							break;
						case Caching_Mode::READ_CACHE:
							if (stat->Type == Utils::Workload_Type::SYNTHETIC) {
							} else {
							}
							break;
						case Caching_Mode::WRITE_CACHE:
							if (stat->Type == Utils::Workload_Type::SYNTHETIC) {
								unsigned int total_pages_accessed = 1;
								switch (stat->Address_distribution_type)
								{
								case Utils::Address_Distribution_Type::STREAMING:
									break;
								case Utils::Address_Distribution_Type::RANDOM_HOTCOLD:
									break;
								case Utils::Address_Distribution_Type::RANDOM_UNIFORM:
									break;
								}
							} else {
							}
							break;
						case Caching_Mode::WRITE_READ_CACHE:
							//Put items on cache based on the accessed addresses
							if (stat->Type == Utils::Workload_Type::SYNTHETIC) {
							} else {
							}
							break;
					}
				}
				break;
			case Cache_Sharing_Mode::EQUAL_PARTITIONING:
				for (auto &stat : workload_stats) {
					switch (caching_mode_per_input_stream[stat->Stream_id])
					{
						case Caching_Mode::TURNED_OFF:
							break;
						case Caching_Mode::READ_CACHE:
							//Put items on cache based on the accessed addresses
							if (stat->Type == Utils::Workload_Type::SYNTHETIC) {
							} else {
							}
							break;
						case Caching_Mode::WRITE_CACHE:
							//Estimate the request arrival rate
							//Estimate the request service rate
							//Estimate the average size of requests in the cache
							//Fillup the cache space based on accessed adddresses to the estimated average cache size
							if (stat->Type == Utils::Workload_Type::SYNTHETIC) {
								//Estimate average write service rate
								unsigned int total_pages_accessed = 1;
								/*double average_write_arrival_rate, stdev_write_arrival_rate;
								double average_read_arrival_rate, stdev_read_arrival_rate;
								double average_write_service_time, average_read_service_time;*/
								switch (stat->Address_distribution_type)
								{
									case Utils::Address_Distribution_Type::STREAMING:
										break;
									case Utils::Address_Distribution_Type::RANDOM_HOTCOLD:
										break;
									case Utils::Address_Distribution_Type::RANDOM_UNIFORM:
										break;
								}
							} else {
							}
							break;
						case Caching_Mode::WRITE_READ_CACHE:
							//Put items on cache based on the accessed addresses
							if (stat->Type == Utils::Workload_Type::SYNTHETIC) {
							} else {
							}
							break;
					}
				}
				break;
		}
	}

	void Data_Cache_Manager_Flash_Advanced::process_new_user_request(User_Request* user_request)
	{
		//This condition shouldn't happen, but we check it
		if (user_request->Transaction_list.size() == 0) {
			return;
		}

		/* 更新实时命中率的变化情况 */
		if (Simulator->Time() > current_time_point) {
			// stream_id_type stream_id = user_request->Stream_id;
			// double read_hit_ratio = (cache_transaction_read_num != 0) ? (cache_transaction_read_hit_num * 1.0 / cache_transaction_read_num) : 0;
			// double write_hit_ratio = (cache_transaction_write_num != 0) ? (cache_transaction_write_hit_num * 1.0 / cache_transaction_write_num) : 0;
			// double hit_ratio = ((cache_transaction_write_num + cache_transaction_read_num) != 0) ? ((cache_transaction_write_hit_num + cache_transaction_read_hit_num) * 1.0 / (cache_transaction_write_num + cache_transaction_read_num)) : 0;
			// qq("read_hit_ratio:"<<read_hit_ratio<<"  "<<"write_hit_ratio:"<<write_hit_ratio<<"  "<<"hit_ratio:"<<hit_ratio<<"  "<<user_request->band_rar[3]);
			
			// /* 注意这里修改了缓存的模型 */
			// if (user_request->band_rar[1] > (user_request->band_rar[3] * 10) && user_request->band_rar[0] < (user_request->band_rar[2] / 10)) {
			// 	caching_mode_per_input_stream[user_request->Stream_id] = Caching_Mode::WRITE_READ_CACHE;
			// 	// qq("R_bandwidth:"<<user_request->band_rar[0]<<"W_bandwidth:"<<user_request->band_rar[2]<<"R_RAR:"<<user_request->band_rar[1]<<"W_RAR:"<<user_request->band_rar[3])
			// }
			// else {
			// 	caching_mode_per_input_stream[user_request->Stream_id] = Caching_Mode::WRITE_CACHE;
			// }
			// qq("R_bandwidth:"<<user_request->band_rar[0]<<"W_bandwidth:"<<user_request->band_rar[2]<<"R_RAR:"<<user_request->band_rar[1]<<"W_RAR:"<<user_request->band_rar[3])
			// wait(1);

			// cache_transaction_read_num[stream_id] = 0, cache_transaction_write_num[stream_id] = 0;
			// cache_transaction_read_hit_num[stream_id] = 0, cache_transaction_write_hit_num[stream_id] = 0;

			// current_time_point = Simulator->Time() + real_time_interval;
		}




		if (user_request->Type == UserRequestType::READ) {
			switch (caching_mode_per_input_stream[user_request->Stream_id]) {
				case Caching_Mode::TURNED_OFF:
					static_cast<FTL*>(nvm_firmware)->Address_Mapping_Unit->Translate_lpa_to_ppa_and_dispatch(user_request->Transaction_list);
					return;
				case Caching_Mode::WRITE_CACHE:
				case Caching_Mode::READ_CACHE:  /* 这里没有对在cache中得不到服务的read事务将其内容从闪存写入cache 而是等到在handle_transaction_serviced_signal_from_PHY中得到读来的数据时候再放 */
				case Caching_Mode::WRITE_READ_CACHE:
				{
					std::list<NVM_Transaction*>::iterator it = user_request->Transaction_list.begin();
					while (it != user_request->Transaction_list.end()) {
						NVM_Transaction_Flash_RD* tr = (NVM_Transaction_Flash_RD*)(*it);
						cache_transaction_read_num[user_request->Stream_id]++;
						if (per_stream_cache[tr->Stream_id]->Exists(tr->Stream_id, tr->LPA)) {  /* 缓存命中 */
							cache_transaction_read_hit_num[user_request->Stream_id]++;
							page_status_type available_sectors_bitmap = per_stream_cache[tr->Stream_id]->Get_slot(tr->Stream_id, tr->LPA).State_bitmap_of_existing_sectors & tr->read_sectors_bitmap;
							if (available_sectors_bitmap == tr->read_sectors_bitmap) {  /* 事务所有的sectors可以得到服务 */
								user_request->Sectors_serviced_from_cache += count_sector_no_from_status_bitmap(tr->read_sectors_bitmap);
								user_request->Transaction_list.erase(it++);//the ++ operation should happen here, otherwise the iterator will be part of the list after erasing it from the list
							} else if (available_sectors_bitmap != 0) {
								user_request->Sectors_serviced_from_cache += count_sector_no_from_status_bitmap(available_sectors_bitmap);  /* 事务部分的sectors(available_sectors_bitmap)可以得到服务 */
								tr->read_sectors_bitmap = (tr->read_sectors_bitmap & ~available_sectors_bitmap);  /* 剩余sectors没有在此次事务中得到缓存服务 */
								tr->Data_and_metadata_size_in_byte -= count_sector_no_from_status_bitmap(available_sectors_bitmap) * SECTOR_SIZE_IN_BYTE;
								it++;
							} else {
								it++;
							}
						} else {  /* 缓存缺失 */
							it++;
						}
					}

					if (user_request->Sectors_serviced_from_cache > 0) {  /* 如果部分读事务可以在DRAM中完成 */ 
						Memory_Transfer_Info* transfer_info = new Memory_Transfer_Info;
						transfer_info->Size_in_bytes = user_request->Sectors_serviced_from_cache * SECTOR_SIZE_IN_BYTE;
						transfer_info->Related_request = user_request;
						transfer_info->next_event_type = Data_Cache_Simulation_Event_Type::MEMORY_READ_FOR_USERIO_FINISHED;
						transfer_info->Stream_id = user_request->Stream_id;
						service_dram_access_request(transfer_info);   /* 注册事件建立缓存访问 */
					}
					if (user_request->Transaction_list.size() > 0) {  /* 剩下的全部都是没fa在DRAM中得到完全服务的读事务 需要把这些事务执行完一个用户请求才算执行完毕 */ 
						static_cast<FTL*>(nvm_firmware)->Address_Mapping_Unit->Translate_lpa_to_ppa_and_dispatch(user_request->Transaction_list);
					}

					return;
				}
			}
		}
		else//This is a write request
		{
			switch (caching_mode_per_input_stream[user_request->Stream_id])
			{
				case Caching_Mode::TURNED_OFF:
				case Caching_Mode::READ_CACHE:
					static_cast<FTL*>(nvm_firmware)->Address_Mapping_Unit->Translate_lpa_to_ppa_and_dispatch(user_request->Transaction_list);
					return;
				case Caching_Mode::WRITE_CACHE://The data cache manger unit performs like a destage buffer
				case Caching_Mode::WRITE_READ_CACHE:
				{
					write_to_destage_buffer(user_request);

					int queue_id = user_request->Stream_id;
					if (shared_dram_request_queue) {
						queue_id = 0;
					}
					if (user_request->Transaction_list.size() > 0) {
						waiting_user_requests_queue_for_dram_free_slot[queue_id].push_back(user_request);

						// if (waiting_user_requests_queue_for_dram_free_slot[queue_id].size() > wait_size) {
						// 	wait_size = waiting_user_requests_queue_for_dram_free_slot[queue_id].size();
						// 	qq(wait_size);
						// }
					}
					return;
				}
			}
		}
	}

	void Data_Cache_Manager_Flash_Advanced::write_to_destage_buffer(User_Request* user_request)
	{
		//To eliminate race condition, MQSim assumes the management information and user data are stored in separate DRAM modules
		unsigned int cache_eviction_read_size_in_sectors = 0;//The size of data evicted from cache
		unsigned int flash_written_back_write_size_in_sectors = 0;//The size of data that is both written back to flash and written to DRAM
		unsigned int dram_write_size_in_sectors = 0;//The size of data written to DRAM (must be >= flash_written_back_write_size_in_sectors)
		std::list<NVM_Transaction*>* evicted_cache_slots = new std::list<NVM_Transaction*>;
		std::list<NVM_Transaction*> writeback_transactions;
		auto it = user_request->Transaction_list.begin();

		int queue_id = user_request->Stream_id;
		if (shared_dram_request_queue) {
			queue_id = 0;
		}

		while (it != user_request->Transaction_list.end() 
			&& (back_pressure_buffer_depth[queue_id] + cache_eviction_read_size_in_sectors + flash_written_back_write_size_in_sectors) < back_pressure_buffer_max_depth) {
			NVM_Transaction_Flash_WR* tr = (NVM_Transaction_Flash_WR*)(*it);
			//If the logical address already exists in the cache
			cache_transaction_write_num[user_request->Stream_id]++;
			if (per_stream_cache[tr->Stream_id]->Exists(tr->Stream_id, tr->LPA)) {  /* 缓存命中 */
				/*MQSim should get rid of writting stale data to the cache.
				* This situation may result from out-of-order transaction execution*/
				cache_transaction_write_hit_num[user_request->Stream_id]++;
				Data_Cache_Slot_Type slot = per_stream_cache[tr->Stream_id]->Get_slot(tr->Stream_id, tr->LPA);  /* 只负责将slot提到队列最前面 */
				sim_time_type timestamp = slot.Timestamp;
				NVM::memory_content_type content = slot.Content;
				if (tr->DataTimeStamp > timestamp) {  /* 只有tr的时间戳大于当前缓存内的时间戳时才能更新content tr的时间戳在盘设备接口处的segment_user_request内赋予当时的时间戳 */
					timestamp = tr->DataTimeStamp;
					content = tr->Content;
				}
				/* 下面一步设置为DIRTY_NO_FLASH_WRITEBACK */ 
				per_stream_cache[tr->Stream_id]->Update_data(tr->Stream_id, tr->LPA, content, timestamp, tr->write_sectors_bitmap | slot.State_bitmap_of_existing_sectors);
			} else {//the logical address is not in the cache  /* 缓存缺失 */
				if (!per_stream_cache[tr->Stream_id]->Check_free_slot_availability()) {
					Data_Cache_Slot_Type evicted_slot = per_stream_cache[tr->Stream_id]->Evict_one_slot_lru();  /* 可以修改替换策略 Evict_one_slot_lru / Evict_one_dirty_slot */ 
					if (evicted_slot.Status == Cache_Slot_Status::DIRTY_NO_FLASH_WRITEBACK) {  /* 如果是Evict_one_dirty_slot 则evicted_slot.Status可能是EMPTY 暂时没有遇到error */
						evicted_cache_slots->push_back(new NVM_Transaction_Flash_WR(Transaction_Source_Type::CACHE,
							tr->Stream_id, count_sector_no_from_status_bitmap(evicted_slot.State_bitmap_of_existing_sectors) * SECTOR_SIZE_IN_BYTE,
							evicted_slot.LPA, NULL, IO_Flow_Priority_Class::URGENT, evicted_slot.Content, evicted_slot.State_bitmap_of_existing_sectors, evicted_slot.Timestamp));
						
						// /* zy1024cs  将脏页刷回至后端flash  测试ID_220.csv的时候会抛出Segmentation fault (core dumped)错误，事实上 在丢进evicted_cache_slots时会考虑刷回(在Execute_simulator_event时) 所以这里写的刷回没有必要*/
						// NVM_Transaction_Flash_WR* tr_wb = new NVM_Transaction_Flash_WR(Transaction_Source_Type::USERIO,   /* 如果换成 USERIO ，总的端到端延迟不会改变，但是会记录写相关的调度信息 */
						// 	tr->Stream_id, count_sector_no_from_status_bitmap(evicted_slot.State_bitmap_of_existing_sectors) * SECTOR_SIZE_IN_BYTE,
						// 	evicted_slot.LPA, NULL, IO_Flow_Priority_Class::URGENT, evicted_slot.Content, evicted_slot.State_bitmap_of_existing_sectors, evicted_slot.Timestamp);
						// flash_written_back_write_size_in_sectors += count_sector_no_from_status_bitmap(tr_wb->write_sectors_bitmap);
						// writeback_transactions.push_back(tr_wb);


						cache_eviction_read_size_in_sectors += count_sector_no_from_status_bitmap(evicted_slot.State_bitmap_of_existing_sectors);
						//DEBUG2("Evicting page" << evicted_slot.LPA << " from write buffer ")
					}
				}
				per_stream_cache[tr->Stream_id]->Insert_write_data(tr->Stream_id, tr->LPA, tr->Content, tr->DataTimeStamp, tr->write_sectors_bitmap);
			}
			dram_write_size_in_sectors += count_sector_no_from_status_bitmap(tr->write_sectors_bitmap);

			/* hot/cold data separation 如果开启冷热分类 下面的也要打开 (因为事务来源于用户 所以这种急切的写回属于用户事务) 感觉是一种在一段时间内对于首次访问就写穿，对于其他的就写回 */
			if (bloom_filter[tr->Stream_id].find(tr->LPA) == bloom_filter[tr->Stream_id].end()) {    /* 将近期没有被访问的事务赶快刷回至flash */
				per_stream_cache[tr->Stream_id]->Change_slot_status_to_writeback(tr->Stream_id, tr->LPA); //Eagerly write back cold data
				flash_written_back_write_size_in_sectors += count_sector_no_from_status_bitmap(tr->write_sectors_bitmap);
				bloom_filter[user_request->Stream_id].insert(tr->LPA);
				writeback_transactions.push_back(tr);
			}

			user_request->Transaction_list.erase(it++);   /* 当back_pressure_buffer_max_depth到达一定量时 就无法删除了 每个写事务要么在缓存中更新 要么强行插入 */
		}
		
		user_request->Sectors_serviced_from_cache += dram_write_size_in_sectors;//This is very important update. It is used to decide when all data sectors of a user request are serviced
		back_pressure_buffer_depth[queue_id] += cache_eviction_read_size_in_sectors + flash_written_back_write_size_in_sectors;

		//Issue memory read for cache evictions
		if (evicted_cache_slots->size() > 0) {
			Memory_Transfer_Info* read_transfer_info = new Memory_Transfer_Info;
			read_transfer_info->Size_in_bytes = cache_eviction_read_size_in_sectors * SECTOR_SIZE_IN_BYTE;
			read_transfer_info->Related_request = evicted_cache_slots;
			read_transfer_info->next_event_type = Data_Cache_Simulation_Event_Type::MEMORY_READ_FOR_CACHE_EVICTION_FINISHED;
			read_transfer_info->Stream_id = user_request->Stream_id;
			service_dram_access_request(read_transfer_info);
		}

		//Issue memory write to write data to DRAM  /* 当用户写请求在缓存中得到全部服务即可认为用户请求完成 无需等待writeback_transactions 但是writeback_transactions太多会影响back_pressure_buffer 因此也会对缓存腾出空间给写事务间接造成影响 */
		if (dram_write_size_in_sectors) {
			Memory_Transfer_Info* write_transfer_info = new Memory_Transfer_Info;
			write_transfer_info->Size_in_bytes = dram_write_size_in_sectors * SECTOR_SIZE_IN_BYTE;
			write_transfer_info->Related_request = user_request;   /* 会在Execute_simulator_event时候检查是否对应的事务列表被全部完成 */
			write_transfer_info->next_event_type = Data_Cache_Simulation_Event_Type::MEMORY_WRITE_FOR_USERIO_FINISHED;
			write_transfer_info->Stream_id = user_request->Stream_id;
			service_dram_access_request(write_transfer_info);
		}

		//If any writeback should be performed, then issue flash write transactions
		if (writeback_transactions.size() > 0) {   /* 写穿的是当前来的用户请求中的事务 因此不需要从缓存中读数据 因为无需建立单独的访问缓存事件 */
			static_cast<FTL*>(nvm_firmware)->Address_Mapping_Unit->Translate_lpa_to_ppa_and_dispatch(writeback_transactions);
		}

		/* Reset control data structures used for hot/cold separation（过去一段时间内未访问的数据直接写穿）  如果开启 上面的也要打开 */
		if (Simulator->Time() > next_bloom_filter_reset_milestone) {
			bloom_filter[user_request->Stream_id].clear();
			next_bloom_filter_reset_milestone = Simulator->Time() + bloom_filter_reset_step;  /* bloom_filter_reset_step = 1000000000 */
		}
	}

	void Data_Cache_Manager_Flash_Advanced::handle_transaction_serviced_signal_from_PHY(NVM_Transaction_Flash* transaction)
	{
		//First check if the transaction source is a user request or the cache itself
		if (transaction->Source != Transaction_Source_Type::USERIO && transaction->Source != Transaction_Source_Type::CACHE) {
			return;
		}

		if (transaction->Source == Transaction_Source_Type::USERIO) {
			_my_instance->broadcast_user_memory_transaction_serviced_signal(transaction);  /* 在host对该完成的事务进行时间的统计 */
		}
		/* This is an update read (a read that is generated for a write request that partially updates page data).
		*  An update read transaction is issued in Address Mapping Unit, but is consumed in data cache manager.*/
		if (transaction->Type == Transaction_Type::READ) {
			if (((NVM_Transaction_Flash_RD*)transaction)->RelatedWrite != NULL) {  /* 针对GC的读的相关操作 */
				((NVM_Transaction_Flash_RD*)transaction)->RelatedWrite->RelatedRead = NULL;
				return;
			}

			switch (Data_Cache_Manager_Flash_Advanced::caching_mode_per_input_stream[transaction->Stream_id])
			{
				case Caching_Mode::TURNED_OFF:
				case Caching_Mode::WRITE_CACHE:
					transaction->UserIORequest->Transaction_list.remove(transaction);  /* 从PHY来的这些事务都是已经执行完毕的 可以删除 */
					if (_my_instance->is_user_request_finished(transaction->UserIORequest)) {
						_my_instance->broadcast_user_request_serviced_signal(transaction->UserIORequest);
					}
					break;
				case Caching_Mode::READ_CACHE:
				case Caching_Mode::WRITE_READ_CACHE:
				{/* 可以暂时不管下面的逻辑代码 */
					if (((Data_Cache_Manager_Flash_Advanced*)_my_instance)->per_stream_cache[transaction->Stream_id]->Exists(transaction->Stream_id, transaction->LPA)) {
						/*MQSim should get rid of writting stale data to the cache.
						* This situation may result from out-of-order transaction execution*/
						Data_Cache_Slot_Type slot = ((Data_Cache_Manager_Flash_Advanced*)_my_instance)->per_stream_cache[transaction->Stream_id]->Get_slot(transaction->Stream_id, transaction->LPA);
						sim_time_type timestamp = slot.Timestamp;
						NVM::memory_content_type content = slot.Content;
						if (((NVM_Transaction_Flash_RD*)transaction)->DataTimeStamp > timestamp) {
							timestamp = ((NVM_Transaction_Flash_RD*)transaction)->DataTimeStamp;
							content = ((NVM_Transaction_Flash_RD*)transaction)->Content;
						}

						((Data_Cache_Manager_Flash_Advanced*)_my_instance)->per_stream_cache[transaction->Stream_id]->Update_data(transaction->Stream_id, transaction->LPA, content,
							timestamp, ((NVM_Transaction_Flash_RD*)transaction)->read_sectors_bitmap | slot.State_bitmap_of_existing_sectors);
					} else {
						if (!((Data_Cache_Manager_Flash_Advanced*)_my_instance)->per_stream_cache[transaction->Stream_id]->Check_free_slot_availability()) {
							std::list<NVM_Transaction*>* evicted_cache_slots = new std::list<NVM_Transaction*>;
							Data_Cache_Slot_Type evicted_slot = ((Data_Cache_Manager_Flash_Advanced*)_my_instance)->per_stream_cache[transaction->Stream_id]->Evict_one_slot_lru();  /* 可以修改替换策略 Evict_one_slot_lru / Evict_one_dirty_slot */
							if (evicted_slot.Status == Cache_Slot_Status::DIRTY_NO_FLASH_WRITEBACK) {
								Memory_Transfer_Info* transfer_info = new Memory_Transfer_Info;
								transfer_info->Size_in_bytes = count_sector_no_from_status_bitmap(evicted_slot.State_bitmap_of_existing_sectors) * SECTOR_SIZE_IN_BYTE;
								evicted_cache_slots->push_back(new NVM_Transaction_Flash_WR(Transaction_Source_Type::USERIO,
									transaction->Stream_id, transfer_info->Size_in_bytes, evicted_slot.LPA, NULL, IO_Flow_Priority_Class::UNDEFINED, evicted_slot.Content,
									evicted_slot.State_bitmap_of_existing_sectors, evicted_slot.Timestamp));
								transfer_info->Related_request = evicted_cache_slots;
								transfer_info->next_event_type = Data_Cache_Simulation_Event_Type::MEMORY_READ_FOR_CACHE_EVICTION_FINISHED;
								transfer_info->Stream_id = transaction->Stream_id;
								unsigned int cache_eviction_read_size_in_sectors = count_sector_no_from_status_bitmap(evicted_slot.State_bitmap_of_existing_sectors);
								int sharing_id = transaction->Stream_id;
								if (((Data_Cache_Manager_Flash_Advanced*)_my_instance)->shared_dram_request_queue) {
									sharing_id = 0;
								}
								((Data_Cache_Manager_Flash_Advanced*)_my_instance)->back_pressure_buffer_depth[sharing_id] += cache_eviction_read_size_in_sectors;
								((Data_Cache_Manager_Flash_Advanced*)_my_instance)->service_dram_access_request(transfer_info);
							}
						}
						((Data_Cache_Manager_Flash_Advanced*)_my_instance)->per_stream_cache[transaction->Stream_id]->Insert_read_data(transaction->Stream_id, transaction->LPA,
							((NVM_Transaction_Flash_RD*)transaction)->Content, ((NVM_Transaction_Flash_RD*)transaction)->DataTimeStamp, ((NVM_Transaction_Flash_RD*)transaction)->read_sectors_bitmap);

						Memory_Transfer_Info* transfer_info = new Memory_Transfer_Info;
						transfer_info->Size_in_bytes = count_sector_no_from_status_bitmap(((NVM_Transaction_Flash_RD*)transaction)->read_sectors_bitmap) * SECTOR_SIZE_IN_BYTE;
						transfer_info->next_event_type = Data_Cache_Simulation_Event_Type::MEMORY_WRITE_FOR_CACHE_FINISHED;
						transfer_info->Stream_id = transaction->Stream_id;
						((Data_Cache_Manager_Flash_Advanced*)_my_instance)->service_dram_access_request(transfer_info);
					}

					transaction->UserIORequest->Transaction_list.remove(transaction);
					if (_my_instance->is_user_request_finished(transaction->UserIORequest)) {
						_my_instance->broadcast_user_request_serviced_signal(transaction->UserIORequest);
					}
					break;
				}
			}
		} else {//This is a write request
			switch (Data_Cache_Manager_Flash_Advanced::caching_mode_per_input_stream[transaction->Stream_id])
			{
				case Caching_Mode::TURNED_OFF:
				case Caching_Mode::READ_CACHE:
					transaction->UserIORequest->Transaction_list.remove(transaction);
					if (_my_instance->is_user_request_finished(transaction->UserIORequest)) {
						_my_instance->broadcast_user_request_serviced_signal(transaction->UserIORequest);
					}
					break;
				case Caching_Mode::WRITE_CACHE:
				case Caching_Mode::WRITE_READ_CACHE:
				{
					int sharing_id = transaction->Stream_id;
					if (((Data_Cache_Manager_Flash_Advanced*)_my_instance)->shared_dram_request_queue) {
						sharing_id = 0;
					}
					((Data_Cache_Manager_Flash_Advanced*)_my_instance)->back_pressure_buffer_depth[sharing_id] -= transaction->Data_and_metadata_size_in_byte / SECTOR_SIZE_IN_BYTE + (transaction->Data_and_metadata_size_in_byte % SECTOR_SIZE_IN_BYTE == 0 ? 0 : 1);

					if (((Data_Cache_Manager_Flash_Advanced*)_my_instance)->per_stream_cache[transaction->Stream_id]->Exists(transaction->Stream_id, ((NVM_Transaction_Flash_WR*)transaction)->LPA)) {
						Data_Cache_Slot_Type slot = ((Data_Cache_Manager_Flash_Advanced*)_my_instance)->per_stream_cache[transaction->Stream_id]->Get_slot(transaction->Stream_id, ((NVM_Transaction_Flash_WR*)transaction)->LPA);
						sim_time_type timestamp = slot.Timestamp;
						NVM::memory_content_type content = slot.Content;
						if (((NVM_Transaction_Flash_WR*)transaction)->DataTimeStamp >= timestamp) {  /* 貌似trace一时半会不会执行 */
							((Data_Cache_Manager_Flash_Advanced*)_my_instance)->per_stream_cache[transaction->Stream_id]->Remove_slot(transaction->Stream_id, ((NVM_Transaction_Flash_WR*)transaction)->LPA);
						}
					}
					
					/* 主要对在waiting_user_requests_queue_for_dram_free_slot内的事务进行执行 因为本函数是从PHY来 意味着有事务已经得到了服务 释放了缓存阻塞资源 */
					auto user_request = ((Data_Cache_Manager_Flash_Advanced*)_my_instance)->waiting_user_requests_queue_for_dram_free_slot[sharing_id].begin();
					while (user_request != ((Data_Cache_Manager_Flash_Advanced*)_my_instance)->waiting_user_requests_queue_for_dram_free_slot[sharing_id].end())
					{
						((Data_Cache_Manager_Flash_Advanced*)_my_instance)->write_to_destage_buffer(*user_request);
						if ((*user_request)->Transaction_list.size() == 0) {
							((Data_Cache_Manager_Flash_Advanced*)_my_instance)->waiting_user_requests_queue_for_dram_free_slot[sharing_id].erase(user_request++);
						} else {
							user_request++;
						}
						//The traffic load on the backend is high and the waiting requests cannot be serviced
						if (((Data_Cache_Manager_Flash_Advanced*)_my_instance)->back_pressure_buffer_depth[sharing_id] > ((Data_Cache_Manager_Flash_Advanced*)_my_instance)->back_pressure_buffer_max_depth) {
							break;
						}
					}

					/*if (_my_instance->back_pressure_buffer_depth[sharing_id] < _my_instance->back_pressure_buffer_max_depth)//The traffic load on the backend is low and the waiting requests can be serviced
					{
						std::list<NVM_Transaction*>* evicted_cache_slots = new std::list<NVM_Transaction*>;
						while (!((Data_Cache_Manager_Flash_Advanced*)_my_instance)->per_stream_cache[transaction->Stream_id]->Empty())
						{
							DataCacheSlotType evicted_slot = ((Data_Cache_Manager_Flash_Advanced*)_my_instance)->per_stream_cache[transaction->Stream_id]->Evict_one_dirty_slot();
							if (evicted_slot.Status != CacheSlotStatus::EMPTY)
							{
								evicted_cache_slots->push_back(new NVM_Transaction_Flash_WR(Transaction_Source_Type::CACHE,
									transaction->Stream_id, count_sector_no_from_status_bitmap(evicted_slot.State_bitmap_of_existing_sectors) * SECTOR_SIZE_IN_BYTE,
									evicted_slot.LPA, NULL, IO_Flow_Priority_Class::UNDEFINED, evicted_slot.Content, evicted_slot.State_bitmap_of_existing_sectors, evicted_slot.Timestamp));
								_my_instance->back_pressure_buffer_depth[sharing_id] += count_sector_no_from_status_bitmap(evicted_slot.State_bitmap_of_existing_sectors);
								cache_eviction_read_size_in_sectors += count_sector_no_from_status_bitmap(evicted_slot.State_bitmap_of_existing_sectors);
							}
							else break;
							if (_my_instance->back_pressure_buffer_depth[sharing_id] >= _my_instance->back_pressure_buffer_max_depth)
								break;
						}
						
						if (cache_eviction_read_size_in_sectors > 0)
						{
							Memory_Transfer_Info* read_transfer_info = new Memory_Transfer_Info;
							read_transfer_info->Size_in_bytes = cache_eviction_read_size_in_sectors * SECTOR_SIZE_IN_BYTE;
							read_transfer_info->Related_request = evicted_cache_slots;
							read_transfer_info->next_event_type = Data_Cache_Simulation_Event_Type::MEMORY_READ_FOR_CACHE_EVICTION_FINISHED;
							read_transfer_info->Stream_id = transaction->Stream_id;
							((Data_Cache_Manager_Flash_Advanced*)_my_instance)->service_dram_access_request(read_transfer_info);
						}
					}*/

					break;
				}
			}
		}
	}

	void Data_Cache_Manager_Flash_Advanced::service_dram_access_request(Memory_Transfer_Info* request_info)
	{
		if (memory_channel_is_busy) {
			if(shared_dram_request_queue) {
				dram_execution_queue[0].push(request_info);
			} else {
				dram_execution_queue[request_info->Stream_id].push(request_info);
			}
		} else {
			Simulator->Register_sim_event(Simulator->Time() + estimate_dram_access_time(request_info->Size_in_bytes, dram_row_size,
				dram_busrt_size, dram_burst_transfer_time_ddr, dram_tRCD, dram_tCL, dram_tRP),
				this, request_info, static_cast<int>(request_info->next_event_type));
			memory_channel_is_busy = true;
			dram_execution_list_turn = request_info->Stream_id;
		}
	}

	void Data_Cache_Manager_Flash_Advanced::Execute_simulator_event(MQSimEngine::Sim_Event* ev)
	{
		Data_Cache_Simulation_Event_Type eventType = (Data_Cache_Simulation_Event_Type)ev->Type;
		Memory_Transfer_Info* transfer_info = (Memory_Transfer_Info*)ev->Parameters;

		switch (eventType)
		{
			case Data_Cache_Simulation_Event_Type::MEMORY_READ_FOR_USERIO_FINISHED://A user read is service from DRAM cache
			case Data_Cache_Simulation_Event_Type::MEMORY_WRITE_FOR_USERIO_FINISHED:
				((User_Request*)(transfer_info)->Related_request)->Sectors_serviced_from_cache -= transfer_info->Size_in_bytes / SECTOR_SIZE_IN_BYTE;
				if (is_user_request_finished((User_Request*)(transfer_info)->Related_request))
					broadcast_user_request_serviced_signal(((User_Request*)(transfer_info)->Related_request));
				break;
			case Data_Cache_Simulation_Event_Type::MEMORY_READ_FOR_CACHE_EVICTION_FINISHED://Reading data from DRAM and writing it back to the flash storage  /* 这里对刷回事件进行执行 */
				static_cast<FTL*>(nvm_firmware)->Address_Mapping_Unit->Translate_lpa_to_ppa_and_dispatch(*((std::list<NVM_Transaction*>*)(transfer_info->Related_request)));
				delete (std::list<NVM_Transaction*>*)transfer_info->Related_request;
				break;
			case Data_Cache_Simulation_Event_Type::MEMORY_WRITE_FOR_CACHE_FINISHED://The recently read data from flash is written back to memory to support future user read requests
				break;
		}
		delete transfer_info;

		memory_channel_is_busy = false;
		if (shared_dram_request_queue)	{
			if (dram_execution_queue[0].size() > 0)	{
				Memory_Transfer_Info* transfer_info = dram_execution_queue[0].front();
				dram_execution_queue[0].pop();
				Simulator->Register_sim_event(Simulator->Time() + estimate_dram_access_time(transfer_info->Size_in_bytes, dram_row_size, dram_busrt_size,
					dram_burst_transfer_time_ddr, dram_tRCD, dram_tCL, dram_tRP),
					this, transfer_info, static_cast<int>(transfer_info->next_event_type));
				memory_channel_is_busy = true;
			}
		} else {
			for (unsigned int i = 0; i < stream_count; i++) {
				dram_execution_list_turn++;
				dram_execution_list_turn %= stream_count;
				if (dram_execution_queue[dram_execution_list_turn].size() > 0) {
					Memory_Transfer_Info* transfer_info = dram_execution_queue[dram_execution_list_turn].front();
					dram_execution_queue[dram_execution_list_turn].pop();
					Simulator->Register_sim_event(Simulator->Time() + estimate_dram_access_time(transfer_info->Size_in_bytes, dram_row_size, dram_busrt_size,
						dram_burst_transfer_time_ddr, dram_tRCD, dram_tCL, dram_tRP),
						this, transfer_info, static_cast<int>(transfer_info->next_event_type));
					memory_channel_is_busy = true;
					break;
				}
			}
		}
	}


}

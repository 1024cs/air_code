#include "Data_Cache_Manager_Base.h"
#include "FTL.h"

namespace SSD_Components
{
	Data_Cache_Manager_Base* Data_Cache_Manager_Base::_my_instance = NULL;
	Caching_Mode* Data_Cache_Manager_Base::caching_mode_per_input_stream;

	Data_Cache_Manager_Base::Data_Cache_Manager_Base(const sim_object_id_type& id, Host_Interface_Base* host_interface, NVM_Firmware* nvm_firmware,
		unsigned int dram_row_size, unsigned int dram_data_rate, unsigned int dram_busrt_size, sim_time_type dram_tRCD, sim_time_type dram_tCL, sim_time_type dram_tRP,
		Caching_Mode* caching_mode_per_input_stream, Cache_Sharing_Mode sharing_mode, unsigned int stream_count)
		: MQSimEngine::Sim_Object(id), host_interface(host_interface), nvm_firmware(nvm_firmware),
		dram_row_size(dram_row_size), dram_data_rate(dram_data_rate), dram_busrt_size(dram_busrt_size), dram_tRCD(dram_tRCD), dram_tCL(dram_tCL), dram_tRP(dram_tRP),
		sharing_mode(sharing_mode), stream_count(stream_count)
	{
		_my_instance = this;
		dram_burst_transfer_time_ddr = (double) ONE_SECOND / (dram_data_rate * 1000 * 1000);
		this->caching_mode_per_input_stream = new Caching_Mode[stream_count];
		for (unsigned int i = 0; i < stream_count; i++) {
			this->caching_mode_per_input_stream[i] = caching_mode_per_input_stream[i];
		}

		/* zy1024cs */
		this->cache_transaction_read_num = new int[stream_count];
		this->cache_transaction_write_num = new int[stream_count];
		this->cache_transaction_read_hit_num = new int[stream_count];
		this->cache_transaction_write_hit_num = new int[stream_count];
		for (unsigned int i = 0; i < stream_count; i++) {
			this->cache_transaction_read_num[i] = 0;
			this->cache_transaction_write_num[i] = 0;
			this->cache_transaction_read_hit_num[i] = 0;
			this->cache_transaction_write_hit_num[i] = 0;
		}
	}

	Data_Cache_Manager_Base::~Data_Cache_Manager_Base() {}

	void Data_Cache_Manager_Base::Setup_triggers()
	{
		Sim_Object::Setup_triggers();
		host_interface->Connect_to_user_request_arrived_signal(handle_user_request_arrived_signal);   /* 把缓存处理新的用户请求的函数添加到host_interface处，由host_interface进行循环传播调用 貌似只执行一次 在初始化的时候执行 */
	}

	void Data_Cache_Manager_Base::Start_simulation() {}
	
	void Data_Cache_Manager_Base::Validate_simulation_config() {}

	void Data_Cache_Manager_Base::Connect_to_user_request_serviced_signal(UserRequestServicedSignalHanderType function)
	{
		connected_user_request_serviced_signal_handlers.push_back(function);  /* 貌似其中只放host的一个函数 即handle_user_request_serviced_signal_from_cache */
	}
	
	void Data_Cache_Manager_Base::broadcast_user_request_serviced_signal(User_Request* nvm_transaction)
	{
		for (std::vector<UserRequestServicedSignalHanderType>::iterator it = connected_user_request_serviced_signal_handlers.begin();
			it != connected_user_request_serviced_signal_handlers.end(); it++) {
			(*it)(nvm_transaction);
		}
	}

	void Data_Cache_Manager_Base::Connect_to_user_memory_transaction_serviced_signal(MemoryTransactionServicedSignalHanderType function)
	{
		connected_user_memory_transaction_serviced_signal_handlers.push_back(function);  /* 只有host的一个函数 即handle_user_memory_transaction_serviced_signal_from_cache */
	}

	void Data_Cache_Manager_Base::broadcast_user_memory_transaction_serviced_signal(NVM_Transaction* transaction)  /* PHY告知host对缓存中的每个完成的事务进行相关时间统计 Update_transaction_statistics */
	{
		for (std::vector<MemoryTransactionServicedSignalHanderType>::iterator it = connected_user_memory_transaction_serviced_signal_handlers.begin();
			it != connected_user_memory_transaction_serviced_signal_handlers.end(); it++) {
			(*it)(transaction);
		}
	}

	void Data_Cache_Manager_Base::handle_user_request_arrived_signal(User_Request* user_request)  /* 被主机端broadcast_user_request_arrival_signal调用 */
	{
		/* zy1024cs */
		user_request->cache_time = Simulator->Time();
		for (std::list<NVM_Transaction*>::const_iterator it = (user_request->Transaction_list).begin(); it != (user_request->Transaction_list).end(); it++){
			(*it)->cache_time = user_request->cache_time;
		}

		_my_instance->process_new_user_request(user_request);   /* virtual void process_new_user_request */
	}

	void Data_Cache_Manager_Base::Set_host_interface(Host_Interface_Base* host_interface)
	{
		this->host_interface = host_interface;
	}

	/* zy1024cs */
	std::string Data_Cache_Manager_Base::Get_hit_ratio(stream_id_type stream_id) {
		double read_hit_ratio = (cache_transaction_read_num[stream_id] != 0) ? (cache_transaction_read_hit_num[stream_id] * 1.0 / cache_transaction_read_num[stream_id]) : 0;
		double write_hit_ratio = (cache_transaction_write_num[stream_id] != 0) ? (cache_transaction_write_hit_num[stream_id] * 1.0 / cache_transaction_write_num[stream_id]) : 0;
		double hit_ratio = ((cache_transaction_write_num[stream_id] + cache_transaction_read_num[stream_id]) != 0) ? ((cache_transaction_write_hit_num[stream_id] + cache_transaction_read_hit_num[stream_id]) * 1.0 / (cache_transaction_write_num[stream_id] + cache_transaction_read_num[stream_id])) : 0;
		return std::to_string(read_hit_ratio) + "," + std::to_string(write_hit_ratio) + "," + std::to_string(hit_ratio);
	}

	std::string Data_Cache_Manager_Base::Get_hit_ratio() {
		int cache_transaction_read_hit_num_all = 0, cache_transaction_write_hit_num_all = 0;
		int cache_transaction_read_num_all = 0, cache_transaction_write_num_all = 0;
		for (int stream_id = 0; stream_id < stream_count; stream_id++) {
			cache_transaction_read_hit_num_all += cache_transaction_read_hit_num[stream_id];
			cache_transaction_write_hit_num_all += cache_transaction_write_hit_num[stream_id];
			cache_transaction_read_num_all += cache_transaction_read_num[stream_id];
			cache_transaction_write_num_all += cache_transaction_write_num[stream_id];
		}
		double read_hit_ratio = (cache_transaction_read_num_all != 0) ? (cache_transaction_read_hit_num_all * 1.0 / cache_transaction_read_num_all) : 0;
		double write_hit_ratio = (cache_transaction_write_num_all != 0) ? (cache_transaction_write_hit_num_all * 1.0 / cache_transaction_write_num_all) : 0;
		double hit_ratio = ((cache_transaction_write_num_all + cache_transaction_read_num_all) != 0) ? ((cache_transaction_write_hit_num_all + cache_transaction_read_hit_num_all) * 1.0 / (cache_transaction_write_num_all + cache_transaction_read_num_all)) : 0;
		return std::to_string(read_hit_ratio) + "," + std::to_string(write_hit_ratio) + "," + std::to_string(hit_ratio);
	}

}

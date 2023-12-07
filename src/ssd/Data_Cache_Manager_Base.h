#ifndef DATA_CACHE_MANAGER_BASE_H
#define DATA_CACHE_MANAGER_BASE_H

#include <vector>
#include "../sim/Sim_Object.h"
#include "Host_Interface_Base.h"
#include "User_Request.h"
#include "NVM_Firmware.h"
#include "NVM_PHY_ONFI.h"
#include "../utils/Workload_Statistics.h"

namespace SSD_Components
{
	class NVM_Firmware;
	class Host_Interface_Base;
	enum class Caching_Mode {WRITE_CACHE, READ_CACHE, WRITE_READ_CACHE, TURNED_OFF};
	enum class Caching_Mechanism { SIMPLE, ADVANCED, Justitia, Catcher, Fuzzy, RWCoC };
	//How the cache space is shared among the concurrently running I/O flows/streams
	enum class Cache_Sharing_Mode { SHARED,//each application has access to the entire cache space
		EQUAL_PARTITIONING}; 
	class Data_Cache_Manager_Base: public MQSimEngine::Sim_Object
	{
		friend class Data_Cache_Manager_Flash_Advanced;
		friend class Data_Cache_Manager_Flash_Simple;
		friend class Data_Cache_Manager_Flash_Justitia;
		friend class Data_Cache_Manager_Flash_Catcher;
		friend class Data_Cache_Manager_Flash_Fuzzy;
		friend class Data_Cache_Manager_Flash_RWCoC;
	public:
		Data_Cache_Manager_Base(const sim_object_id_type& id, Host_Interface_Base* host_interface, NVM_Firmware* nvm_firmware,
			unsigned int dram_row_size, unsigned int dram_data_rate, unsigned int dram_busrt_size, sim_time_type dram_tRCD, sim_time_type dram_tCL, sim_time_type dram_tRP,
			Caching_Mode* caching_mode_per_input_stream, Cache_Sharing_Mode sharing_mode, unsigned int stream_count);
		virtual ~Data_Cache_Manager_Base();
		void Setup_triggers();
		void Start_simulation();
		void Validate_simulation_config();

		typedef void(*UserRequestServicedSignalHanderType) (User_Request*);
		void Connect_to_user_request_serviced_signal(UserRequestServicedSignalHanderType);  /* host的Setup_triggers中调度 貌似其中只放host的一个函数 即handle_user_request_serviced_signal_from_cache */
		typedef void(*MemoryTransactionServicedSignalHanderType) (NVM_Transaction*);
		void Connect_to_user_memory_transaction_serviced_signal(MemoryTransactionServicedSignalHanderType);  /* host的Setup_triggers中调度 只有host的一个函数 即handle_user_memory_transaction_serviced_signal_from_cache */
		void Set_host_interface(Host_Interface_Base* host_interface);
		virtual void Do_warmup(std::vector<Utils::Workload_Statistics*> workload_stats) = 0;

		/* zy1024cs */
		Caching_Mechanism cache_mechanism;  /* 缓存管理使用的模型 在src/exec/SSD_Device.cpp内被赋值 */
		int *cache_transaction_read_num;
		int *cache_transaction_write_num;
		int *cache_transaction_read_hit_num;
		int *cache_transaction_write_hit_num;
		sim_time_type real_time_interval = 1000000000;
		sim_time_type current_time_point = 0;
		int wait_size = 0;

		std::string Get_hit_ratio(stream_id_type stream_id);
		std::string Get_hit_ratio();


	protected:
		static Data_Cache_Manager_Base* _my_instance;
		Host_Interface_Base* host_interface;
		NVM_Firmware* nvm_firmware;
		unsigned int dram_row_size;//The size of the DRAM rows in bytes
		unsigned int dram_data_rate;//in MT/s
		unsigned int dram_busrt_size;  /* 建立cache访问时间会用到 */
		double dram_burst_transfer_time_ddr;//The transfer time of two bursts, changed from sim_time_type to double to increase precision
		sim_time_type dram_tRCD, dram_tCL, dram_tRP;//DRAM access parameters in nano-seconds
		Cache_Sharing_Mode sharing_mode;
		static Caching_Mode* caching_mode_per_input_stream;
		unsigned int stream_count;

		std::vector<UserRequestServicedSignalHanderType> connected_user_request_serviced_signal_handlers;  /* 貌似只有host的一个函数 即handle_user_request_serviced_signal_from_cache */
		void broadcast_user_request_serviced_signal(User_Request* user_request);  /* 在缓存处服务完成后将服务完成的消息告知主机端 主机端进行下一步操作如对于读请求返回数据给host */

		std::vector<MemoryTransactionServicedSignalHanderType> connected_user_memory_transaction_serviced_signal_handlers;  /* 只有host的一个函数 即handle_user_memory_transaction_serviced_signal_from_cache */
		void broadcast_user_memory_transaction_serviced_signal(NVM_Transaction* transaction);  /* PHY告知host对缓存中的每个完成的事务进行相关时间统计 Update_transaction_statistics */

		static void handle_user_request_arrived_signal(User_Request* user_request);  /* Host_Interface提交用户请求到cache的入口 */
		virtual void process_new_user_request(User_Request* user_request) = 0;

		bool is_user_request_finished(const User_Request* user_request) { return (user_request->Transaction_list.size() == 0 && user_request->Sectors_serviced_from_cache == 0); }
	};


	inline sim_time_type estimate_dram_access_time(const unsigned int memory_access_size_in_byte,
		const unsigned int dram_row_size, const unsigned int dram_burst_size_in_bytes, const double dram_burst_transfer_time_ddr,
		const sim_time_type tRCD, const sim_time_type tCL, const sim_time_type tRP)
	{
		if (memory_access_size_in_byte <= dram_row_size) {
			return (sim_time_type)(tRCD + tCL + sim_time_type((double)(memory_access_size_in_byte / dram_burst_size_in_bytes / 2) * dram_burst_transfer_time_ddr));
		}
		else {
			return (sim_time_type)((tRCD + tCL + (sim_time_type)((double)(dram_row_size / dram_burst_size_in_bytes / 2 * dram_burst_transfer_time_ddr) + tRP) * (double)(memory_access_size_in_byte / dram_row_size / 2)) + tRCD + tCL + (sim_time_type)((double)(memory_access_size_in_byte % dram_row_size) / ((double)dram_burst_size_in_bytes * dram_burst_transfer_time_ddr)));
		}
	}
}

#endif // !DATA_CACHE_MANAGER_BASE_H

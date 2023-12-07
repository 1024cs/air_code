#ifndef HOST_INTERFACE_BASE_H
#define HOST_INTERFACE_BASE_H

#include <vector>
#include "../sim/Sim_Object.h"
#include "../sim/Sim_Reporter.h"
#include "../host/PCIe_Switch.h"
#include "../host/PCIe_Message.h"
#include "User_Request.h"
#include "Data_Cache_Manager_Base.h"
#include <stdint.h>
#include <cstring>

namespace Host_Components
{
	class PCIe_Switch;
}

namespace SSD_Components
{
#define COPYDATA(DEST,SRC,SIZE) if(Simulator->Is_integrated_execution_mode()) {DEST = new char[SIZE]; memcpy(DEST, SRC, SIZE);} else DEST = SRC;
#define DELETE_REQUEST_NVME(REQ) \
	delete (Submission_Queue_Entry*)REQ->IO_command_info; \
	if(Simulator->Is_integrated_execution_mode())\
		{if(REQ->Data != NULL) delete[] (char*)REQ->Data;} \
	if(REQ->Transaction_list.size() != 0) PRINT_ERROR("Deleting an unhandled user requests in the host interface! MQSim thinks something is going wrong!")\
	delete REQ;

	class Data_Cache_Manager_Base;
	class Host_Interface_Base;

	class Input_Stream_Base
	{
	public:
		Input_Stream_Base();
		virtual ~Input_Stream_Base();
		unsigned int STAT_number_of_read_requests;
		unsigned int STAT_number_of_write_requests;
		unsigned int STAT_number_of_read_transactions;
		unsigned int STAT_number_of_write_transactions;
		sim_time_type STAT_sum_of_read_transactions_execution_time, STAT_sum_of_read_transactions_transfer_time, STAT_sum_of_read_transactions_waiting_time;
		sim_time_type STAT_sum_of_write_transactions_execution_time, STAT_sum_of_write_transactions_transfer_time, STAT_sum_of_write_transactions_waiting_time;


		/* zy1024cs */
		unsigned int STAT_number_of_read_transactions_via_flash = 0;
		unsigned int STAT_number_of_write_transactions_via_flash = 0;
		sim_time_type STAT_sum_of_read_transactions_cache_time = 0, STAT_sum_of_read_transactions_tsu_time = 0, STAT_sum_of_read_transactions_all_time = 0;
		sim_time_type STAT_sum_of_write_transactions_cache_time = 0, STAT_sum_of_write_transactions_tsu_time = 0, STAT_sum_of_write_transactions_all_time = 0;
	};

	/* zy1024cs */
	class Node {
	public:
		LHA_type Start_LBA;
		unsigned int SizeInSectors;
		sim_time_type Arrival_time;
		Node(LHA_type Start_LBA, unsigned int SizeInSectors, sim_time_type Arrival_time) {this->Start_LBA = Start_LBA, this->SizeInSectors = SizeInSectors, this->Arrival_time = Arrival_time;}
	};

	class Input_Stream_Manager_Base   /* 完成统计等相关工作 会被Input_Stream_Manager_NVMe等继承 */
	{
		friend class Request_Fetch_Unit_Base;
		friend class Request_Fetch_Unit_NVMe;
		friend class Request_Fetch_Unit_SATA;
	public:
		Input_Stream_Manager_Base(Host_Interface_Base* host_interface);
		virtual ~Input_Stream_Manager_Base();
		virtual void Handle_new_arrived_request(User_Request* request) = 0;
		virtual void Handle_arrived_write_data(User_Request* request) = 0;
		virtual void Handle_serviced_request(User_Request* request) = 0;   /* 告知主机端用户请求已经在缓存侧完成服务 会在缓存端的Execute_simulator_event中的broadcast_user_request_serviced_signal被调用 */
		void Update_transaction_statistics(NVM_Transaction* transaction);
		uint32_t Get_average_read_transaction_turnaround_time(stream_id_type stream_id);//in microseconds
		uint32_t Get_average_read_transaction_execution_time(stream_id_type stream_id);//in microseconds
		uint32_t Get_average_read_transaction_transfer_time(stream_id_type stream_id);//in microseconds
		uint32_t Get_average_read_transaction_waiting_time(stream_id_type stream_id);//in microseconds
		uint32_t Get_average_write_transaction_turnaround_time(stream_id_type stream_id);//in microseconds
		uint32_t Get_average_write_transaction_execution_time(stream_id_type stream_id);//in microseconds
		uint32_t Get_average_write_transaction_transfer_time(stream_id_type stream_id);//in microseconds
		uint32_t Get_average_write_transaction_waiting_time(stream_id_type stream_id);//in microseconds


		/* zy1024cs */
		uint32_t Get_average_read_transaction_cache_time(stream_id_type stream_id);//in microseconds
		uint32_t Get_average_read_transaction_tsu_time(stream_id_type stream_id);//in microseconds
		uint32_t Get_average_read_transaction_all_time(stream_id_type stream_id);//in microseconds
		uint32_t Get_average_write_transaction_cache_time(stream_id_type stream_id);//in microseconds
		uint32_t Get_average_write_transaction_tsu_time(stream_id_type stream_id);//in microseconds
		uint32_t Get_average_write_transaction_all_time(stream_id_type stream_id);//in microseconds
		std::string Get_statistics_string(stream_id_type stream_id);
		std::string Get_transaction_statistics_string(stream_id_type stream_id);

		
		sim_time_type next_reset_time = -1;
		sim_time_type last_reset_time = 0;
		sim_time_type reset_step = 60000000000;  /* 1000000000:1s 60000000000:1min */
		std::vector<std::list<int>*> bandwidth;
		double Get_bandwidth(stream_id_type stream_id);
		sim_time_type last_time = 0;

		std::vector<std::list<Node>*> all_node;
		std::vector<std::set<LHA_type>*> rar_set;
		std::vector<std::map<LHA_type, sim_time_type>*> rar_time;
		std::vector<std::map<LHA_type, int>*> rar_frq;
		std::vector<int> rar_arr;
		std::vector<int> band_arr;

		std::vector<std::list<Node>*> all_node_W;
		std::vector<std::set<LHA_type>*> rar_set_W;
		std::vector<std::map<LHA_type, sim_time_type>*> rar_time_W;
		std::vector<std::map<LHA_type, int>*> rar_frq_W;
		std::vector<int> rar_arr_W;
		std::vector<int> band_arr_W;

		void Calculate_and_update(stream_id_type stream_id, User_Request *request);
		


	protected:
		Host_Interface_Base* host_interface;
		virtual void segment_user_request(User_Request* user_request) = 0;
		std::vector<Input_Stream_Base*> input_streams;
	};

	class Request_Fetch_Unit_Base
	{
	public:
		Request_Fetch_Unit_Base(Host_Interface_Base* host_interface);
		virtual ~Request_Fetch_Unit_Base();
		virtual void Fetch_next_request(stream_id_type stream_id) = 0;
		virtual void Fetch_write_data(User_Request* request) = 0;
		virtual void Send_read_data(User_Request* request) = 0;  /* 完成赋值和Send_write_message_to_host */
		virtual void Process_pcie_write_message(uint64_t, void *, unsigned int) = 0; /* 对于NMVe 感觉就是更新提交尾部或者更新完成头部 意味着一个用户请求已经完成 */
		virtual void Process_pcie_read_message(uint64_t, void *, unsigned int) = 0;  /* 对于NMVe 感觉就是从提交队列中读一个新的提交实体 生成新一轮请求 */
	protected:
		enum class DMA_Req_Type { REQUEST_INFO, WRITE_DATA };  /* 感觉是REQUEST_INFO针对读写 而对于写请求在得到消息后还需要再发送一次消息给host去要写数据 即需要WRITE_DATA */
		struct DMA_Req_Item
		{
			DMA_Req_Type Type;
			void * object;
		};
		Host_Interface_Base* host_interface;
		std::list<DMA_Req_Item*> dma_list;
	};

	class Host_Interface_Base : public MQSimEngine::Sim_Object, public MQSimEngine::Sim_Reporter
	{
		friend class Input_Stream_Manager_Base;
		friend class Input_Stream_Manager_NVMe;
		friend class Input_Stream_Manager_SATA;
		friend class Request_Fetch_Unit_Base;
		friend class Request_Fetch_Unit_NVMe;
		friend class Request_Fetch_Unit_SATA;
	public:
		Host_Interface_Base(const sim_object_id_type& id, HostInterface_Types type, LHA_type max_logical_sector_address, 
			unsigned int sectors_per_page, Data_Cache_Manager_Base* cache);
		virtual ~Host_Interface_Base();
		void Setup_triggers();
		void Validate_simulation_config();

		typedef void(*UserRequestArrivedSignalHandlerType) (User_Request*);
		void Connect_to_user_request_arrived_signal(UserRequestArrivedSignalHandlerType function)
		{
			connected_user_request_arrived_signal_handlers.push_back(function);  /* 会把缓存模块中的handle_user_request_arrived_signal添加进去 只会执行一次 */
		}

		void Consume_pcie_message(Host_Components::PCIe_Message* message)
		{
			if (message->Type == Host_Components::PCIe_Message_Type::READ_COMP) {
				request_fetch_unit->Process_pcie_read_message(message->Address, message->Payload, message->Payload_size);
			} else {  /* 此时message->Type 为 Host_Components::PCIe_Message_Type::WRITE_REQ */
				request_fetch_unit->Process_pcie_write_message(message->Address, message->Payload, message->Payload_size);
			}
			delete message;
		}
	
		void Send_read_message_to_host(uint64_t addresss, unsigned int request_read_data_size);  /* 暂时感觉是向host发起一个读出一个提交队列实体到设备端来 */
		void Send_write_message_to_host(uint64_t addresss, void* message, unsigned int message_size);  /* 暂时感觉是吧一个完成队列实体写到host的内存当中去 */

		HostInterface_Types GetType() { return type; }  /* { SATA, NVME } */
		void Attach_to_device(Host_Components::PCIe_Switch* pcie_switch);
		LHA_type Get_max_logical_sector_address();  /* return max_logical_sector_address; */
		unsigned int Get_no_of_LHAs_in_an_NVM_write_unit();  /* return sectors_per_page; */
	// protected:  /* zy1024cs改 */
		HostInterface_Types type;
		LHA_type max_logical_sector_address;
		unsigned int sectors_per_page;
		static Host_Interface_Base* _my_instance;
		Input_Stream_Manager_Base* input_stream_manager;
		Request_Fetch_Unit_Base* request_fetch_unit;
		Data_Cache_Manager_Base* cache;
		std::vector<UserRequestArrivedSignalHandlerType> connected_user_request_arrived_signal_handlers;

		void broadcast_user_request_arrival_signal(User_Request* user_request)   /* 暂时认为该模块是进入缓存的时刻 其中vector内的元素只有一个好像 缓存模块中的handle_user_request_arrived_signal */
		{  /* 在NVMe协议的接口上被Handle_new_arrived_request调用 */
			
			for (std::vector<UserRequestArrivedSignalHandlerType>::iterator it = connected_user_request_arrived_signal_handlers.begin();
				it != connected_user_request_arrived_signal_handlers.end(); it++) {
				(*it)(user_request);
			}
		}

		static void handle_user_request_serviced_signal_from_cache(User_Request* user_request)  /* 被cache调用 缓存处理完用户的所有事务后广播用户请求 */
		{
			_my_instance->input_stream_manager->Handle_serviced_request(user_request);
		}

		static void handle_user_memory_transaction_serviced_signal_from_cache(NVM_Transaction* transaction)  /* 被cache调用 当事务在后端处理完毕后会由PHY通知cache */
		{
			_my_instance->input_stream_manager->Update_transaction_statistics(transaction);
		}
	private:
		Host_Components::PCIe_Switch* pcie_switch;
	};
}

#endif // !HOST_INTERFACE_BASE_H

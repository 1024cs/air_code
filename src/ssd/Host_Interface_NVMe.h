#ifndef HOST_INTERFACE_NVME_H
#define HOST_INTERFACE_NVME_H

#include <vector>
#include <list>
#include <map>
#include "../sim/Sim_Event.h"
#include "Host_Interface_Base.h"
#include "User_Request.h"
#include "Host_Interface_Defs.h"

namespace SSD_Components
{
class Input_Stream_NVMe : public Input_Stream_Base
{	/* 在NVMe协议中 每个用户的I/O负载分别对应于一个提交队列和完成队列 */
public:
	Input_Stream_NVMe(IO_Flow_Priority_Class::Priority priority_class, LHA_type start_logical_sector_address, LHA_type end_logical_sector_address,
					  uint64_t submission_queue_base_address, uint16_t submission_queue_size,
					  uint64_t completion_queue_base_address, uint16_t completion_queue_size) : Input_Stream_Base(),
																								Priority_class(priority_class),
																								Start_logical_sector_address(start_logical_sector_address), End_logical_sector_address(end_logical_sector_address),
																								Submission_queue_base_address(submission_queue_base_address), Submission_queue_size(submission_queue_size),
																								Completion_queue_base_address(completion_queue_base_address), Completion_queue_size(completion_queue_size),
																								Submission_head(0), Submission_head_informed_to_host(0), Submission_tail(0), Completion_head(0), Completion_tail(0), On_the_fly_requests(0) {}
	~Input_Stream_NVMe();
	IO_Flow_Priority_Class::Priority Priority_class;
	LHA_type Start_logical_sector_address;
	LHA_type End_logical_sector_address;
	uint64_t Submission_queue_base_address;
	uint16_t Submission_queue_size;
	uint64_t Completion_queue_base_address;
	uint16_t Completion_queue_size;
	uint16_t Submission_head;
	uint16_t Submission_head_informed_to_host; //To avoide race condition, the submission head must not be informed to host until the request info of the head is successfully arrived
	uint16_t Submission_tail;
	uint16_t Completion_head;
	uint16_t Completion_tail;
	std::list<User_Request *> Waiting_user_requests;		// /* 可能就是Device-level I/O Request Queue */  The list of requests that have been fetch to the device queue and are getting serviced
	std::list<User_Request *> Completed_user_requests;		// The list of requests that are completed but have not been informed to the host due to full CQ
	std::list<User_Request *> Waiting_write_data_transfers; // The list of write requests that are waiting for data
	uint16_t On_the_fly_requests;							// /* 每当Fetch_next_request时+1，每当服务完一个请求对应的所有事务时-1，最大容量不超过Queue_fetch_size(目前为512) */ the number of requests that are either being fetch from host or waiting in the device queue
};

class Input_Stream_Manager_NVMe : public Input_Stream_Manager_Base
{
public:
	Input_Stream_Manager_NVMe(Host_Interface_Base *host_interface, uint16_t queue_fetch_szie);
	unsigned int Queue_fetch_size;   /* 对应于ssdconfig.xml中的Queue_Fetch_Size(目前为512) 满足On_the_fly_requests < Queue_fetch_size */
	stream_id_type Create_new_stream(IO_Flow_Priority_Class::Priority priority_class, LHA_type start_logical_sector_address, LHA_type end_logical_sector_address,
									 uint64_t submission_queue_base_address, uint16_t submission_queue_size,
									 uint64_t completion_queue_base_address, uint16_t completion_queue_size);
	void Submission_queue_tail_pointer_update(stream_id_type stream_id, uint16_t tail_pointer_value);  /* 向主机端发起'读'一个提交实体 On_the_fly_requests++ */
	void Completion_queue_head_pointer_update(stream_id_type stream_id, uint16_t head_pointer_value);
	void Handle_new_arrived_request(User_Request *request);
	void Handle_arrived_write_data(User_Request *request);
	void Handle_serviced_request(User_Request *request);  /* 缓存侧在完成用户请求相关事务时进行广播 通知用户请求全部完成 */
	uint16_t Get_submission_queue_depth(stream_id_type stream_id);
	uint16_t Get_completion_queue_depth(stream_id_type stream_id);
	IO_Flow_Priority_Class::Priority Get_priority_class(stream_id_type stream_id);

private:
	void segment_user_request(User_Request *user_request);
	void inform_host_request_completed(stream_id_type stream_id, User_Request *request);  /* 在src/ssd/Host_Interface_NVMe.cpp中被Completion_queue_head_pointer_update和Handle_serviced_request调用 也就是更新CQ中的一些指针参数 然后Send_write_message_to_host */
};

class Request_Fetch_Unit_NVMe : public Request_Fetch_Unit_Base
{
public:
	Request_Fetch_Unit_NVMe(Host_Interface_Base *host_interface);
	void Fetch_next_request(stream_id_type stream_id);  /* 生成一个DMA_Req_Type::REQUEST_INFO 然后Send_read_message_to_host */
	void Fetch_write_data(User_Request *request);  /* 生成一个DMA_Req_Type::WRITE_DATA 然后Send_read_message_to_host */
	void Send_read_data(User_Request *request);  /* 内部调用Send_write_message_to_host */
	void Send_completion_queue_element(User_Request *request, uint16_t sq_head_value);  /* 生成一个Completion_Queue_Entry 然后调用Send_write_message_to_host */
	void Process_pcie_write_message(uint64_t, void *, unsigned int);  /* 感觉就是更新提交尾部或者更新完成头部 意味着一个用户请求已经完成 */
	void Process_pcie_read_message(uint64_t, void *, unsigned int);  /* 感觉就是从提交队列中读一个新的提交实体 生成新一轮请求 */

private:
	uint16_t current_phase;
	uint32_t number_of_sent_cqe;
};

class Host_Interface_NVMe : public Host_Interface_Base
{
	friend class Input_Stream_Manager_NVMe;
	friend class Request_Fetch_Unit_NVMe;

public:
	Host_Interface_NVMe(const sim_object_id_type &id, LHA_type max_logical_sector_address,
						uint16_t submission_queue_depth, uint16_t completion_queue_depth,
						unsigned int no_of_input_streams, uint16_t queue_fetch_size, unsigned int sectors_per_page, Data_Cache_Manager_Base *cache);
	stream_id_type Create_new_stream(IO_Flow_Priority_Class::Priority priority_class, LHA_type start_logical_sector_address, LHA_type end_logical_sector_address,
									 uint64_t submission_queue_base_address, uint64_t completion_queue_base_address);  /* 在src/exec/Host_System.cpp的Start_simulation被调用 */
	void Start_simulation();
	void Validate_simulation_config();
	void Execute_simulator_event(MQSimEngine::Sim_Event *);
	uint16_t Get_submission_queue_depth();
	uint16_t Get_completion_queue_depth();
	void Report_results_in_XML(std::string name_prefix, Utils::XmlWriter &xmlwriter);

private:
	uint16_t submission_queue_depth, completion_queue_depth;
	unsigned int no_of_input_streams;
};
} // namespace SSD_Components

#endif // !HOSTINTERFACE_NVME_H

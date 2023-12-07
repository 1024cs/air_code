#include "IO_Flow_Trace_Based.h"
#include "../utils/StringTools.h"
#include "ASCII_Trace_Definition.h"
#include "../utils/DistributionTypes.h"

namespace Host_Components
{
IO_Flow_Trace_Based::IO_Flow_Trace_Based(const sim_object_id_type &name, uint16_t flow_id, LHA_type start_lsa_on_device, LHA_type end_lsa_on_device, uint16_t io_queue_id,
										 uint16_t nvme_submission_queue_size, uint16_t nvme_completion_queue_size, IO_Flow_Priority_Class::Priority priority_class, double initial_occupancy_ratio,
										 std::string trace_file_path, Trace_Time_Unit time_unit, unsigned int total_replay_count, unsigned int percentage_to_be_simulated,
										 HostInterface_Types SSD_device_type, PCIe_Root_Complex *pcie_root_complex, SATA_HBA *sata_hba,
										 bool enabled_logging, sim_time_type logging_period, std::string logging_file_path) : IO_Flow_Base(name, flow_id, start_lsa_on_device, end_lsa_on_device, io_queue_id, nvme_submission_queue_size, nvme_completion_queue_size, priority_class, 0, initial_occupancy_ratio, 0, SSD_device_type, pcie_root_complex, sata_hba, enabled_logging, logging_period, logging_file_path),
																															  trace_file_path(trace_file_path), time_unit(time_unit), total_replay_no(total_replay_count), percentage_to_be_simulated(percentage_to_be_simulated),
																															  total_requests_in_file(0), time_offset(0)
{
	if (percentage_to_be_simulated > 100)
	{
		percentage_to_be_simulated = 100;
		PRINT_MESSAGE("Bad value for percentage of trace file! It is set to 100 % ");
	}
}

IO_Flow_Trace_Based::~IO_Flow_Trace_Based()
{
}

Host_IO_Request *IO_Flow_Trace_Based::Generate_next_request()
{
	if (current_trace_line.size() == 0 || STAT_generated_request_count >= total_requests_to_be_generated)
	{
		return NULL;
	}

	Host_IO_Request *request = new Host_IO_Request;
	if (current_trace_line[ASCIITraceTypeColumn].compare(ASCIITraceWriteCode) == 0)
	{
		request->Type = Host_IO_Request_Type::WRITE;
		STAT_generated_write_request_count++;
	}
	else
	{
		request->Type = Host_IO_Request_Type::READ;	
		STAT_generated_read_request_count++;
	}

	char *pEnd;
	request->LBA_count = std::strtoul(current_trace_line[ASCIITraceSizeColumn].c_str(), &pEnd, 0) / 512;

	request->Start_LBA = std::strtoull(current_trace_line[ASCIITraceAddressColumn].c_str(), &pEnd, 0) / 512;
	if (request->Start_LBA <= (end_lsa_on_device - start_lsa_on_device))
	{
		request->Start_LBA += start_lsa_on_device;
	}
	else
	{
		request->Start_LBA = start_lsa_on_device + request->Start_LBA % (end_lsa_on_device - start_lsa_on_device);
	}

	request->Arrival_time = time_offset + Simulator->Time();
	// qq(request->Start_LBA * 512<<"-"<<request->LBA_count * 512<<"-"<<request->Arrival_time);
	STAT_generated_request_count++;

	return request;
}

void IO_Flow_Trace_Based::NVMe_consume_io_request(Completion_Queue_Entry *io_request)
{
	IO_Flow_Base::NVMe_consume_io_request(io_request);  /* 完成一个请求 进行相关统计工作 */
	IO_Flow_Base::NVMe_update_and_submit_completion_queue_tail();
}

void IO_Flow_Trace_Based::SATA_consume_io_request(Host_IO_Request *io_request)
{
	IO_Flow_Base::SATA_consume_io_request(io_request);
}

void IO_Flow_Trace_Based::Start_simulation()
{
	IO_Flow_Base::Start_simulation();
	std::string trace_line;
	char *pEnd;

	trace_file.open(trace_file_path, std::ios::in);
	if (!trace_file.is_open())
	{
		PRINT_ERROR("Error while opening input trace file: " << trace_file_path)
	}
	PRINT_MESSAGE("Investigating input trace file: " << trace_file_path);

	sim_time_type last_request_arrival_time = 0;
	while (std::getline(trace_file, trace_line))
	{
		Utils::Helper_Functions::Remove_cr(trace_line);
		current_trace_line.clear();
		Utils::Helper_Functions::Tokenize(trace_line, ASCIILineDelimiter, current_trace_line);  /* 注意这方式分割后每个元素的末尾基本上都有, 除了最后一个元素和读写 */
		if (current_trace_line.size() != ASCIIItemsPerLine)
		{
			break;
		}
		total_requests_in_file++;
		sim_time_type prev_time = last_request_arrival_time;
		last_request_arrival_time = std::strtoll(current_trace_line[ASCIITraceTimeColumn].c_str(), &pEnd, 10);
		if (last_request_arrival_time < prev_time)
		{
			PRINT_ERROR("Unexpected request arrival time: " << last_request_arrival_time << "\nMQSim expects request arrival times to be monotonically increasing in the input trace!")
		}
	}

	trace_file.close();
	PRINT_MESSAGE("Trace file: " << trace_file_path << " seems healthy");

	if (total_replay_no == 1)
	{
		total_requests_to_be_generated = (int)(((double)percentage_to_be_simulated / 100) * total_requests_in_file);
	}
	else
	{
		total_requests_to_be_generated = total_requests_in_file * total_replay_no;
	}

	sim_time_type curFireTime = 0;
	trace_file.open(trace_file_path);
	current_trace_line.clear();
	std::getline(trace_file, trace_line);
	Utils::Helper_Functions::Remove_cr(trace_line);
	Utils::Helper_Functions::Tokenize(trace_line, ASCIILineDelimiter, current_trace_line);
	// Simulator->Register_sim_event(std::strtoll(current_trace_line[ASCIITraceTimeColumn].c_str(), &pEnd, 10), this);
	curFireTime = std::strtoll(current_trace_line[ASCIITraceTimeColumn].c_str(), &pEnd, 10);
	//convert trace time to simulator time 
	switch(time_unit){
		case Trace_Time_Unit::PICOSECOND:
			curFireTime = curFireTime / PicoSec2SimTimeCoeff;
			break;
		case Trace_Time_Unit::NANOSECOND:
			curFireTime = curFireTime * NanoSec2SimTimeCoeff;
			break;
		case Trace_Time_Unit::MICROSECOND:
			curFireTime = curFireTime * MicroSec2SimTimeCoeff;
			break;
		default:
			PRINT_ERROR("Unexpected Trace Time Unit!")
	}
	// qq("curFireTime"<<curFireTime);
	// qq("time"<<Simulator->Time());
	// qq("time_offset"<<time_offset);

	/* zy1024cs */
	if (STAT_serviced_request_count == 0)
		first_req_time = curFireTime;

	Simulator->Register_sim_event(curFireTime, this);
}

void IO_Flow_Trace_Based::Validate_simulation_config()
{
}

void IO_Flow_Trace_Based::Execute_simulator_event(MQSimEngine::Sim_Event *)
{
	/* 大概就是每次根据current_trace_line内的时间设置下一次发出请求的时间(因此下次触发时的Simulator->Time()就是上一次注册事件使用的时间) 然后触发后就可以将current_trace_line内容提取出来去在Generate_next_request中完成 之后继续设置下一次触发的时间 */
	Host_IO_Request *request = Generate_next_request();   /* request->Arrival_time = time_offset + Simulator->Time(); 用的上一个current_trace_line */
	if (request != NULL)
	{
		Submit_io_request(request);  /* request->Enqueue_time = Simulator->Time(); 有条件if (!NVME_SQ_FULL(nvme_queue_pair) && available_command_ids.size() > 0)  */
	}

	if (STAT_generated_request_count < total_requests_to_be_generated)
	{
		std::string trace_line;
		if (std::getline(trace_file, trace_line))   /* 赋值一行给trace_line */
		{
			Utils::Helper_Functions::Remove_cr(trace_line);   /* 去掉\n */
			current_trace_line.clear();
			Utils::Helper_Functions::Tokenize(trace_line, ASCIILineDelimiter, current_trace_line);    /* 每个元素丢到新的current_trace_line，和上面不一样 */
		}
		else
		{   /* 一般不会执行下面 */
			trace_file.close();
			trace_file.open(trace_file_path);
			replay_counter++;
			time_offset = Simulator->Time();    /* 注意这里更新了time_offset */
			std::getline(trace_file, trace_line);
			Utils::Helper_Functions::Remove_cr(trace_line);
			current_trace_line.clear();
			Utils::Helper_Functions::Tokenize(trace_line, ASCIILineDelimiter, current_trace_line);
			PRINT_MESSAGE("* Replay round " << replay_counter << "of " << total_replay_no << " started  for" << ID())
		}
		char *pEnd;
		// Simulator->Register_sim_event(time_offset + std::strtoll(current_trace_line[ASCIITraceTimeColumn].c_str(), &pEnd, 10), this);
		sim_time_type curFireTime = 0;
		curFireTime = std::strtoll(current_trace_line[ASCIITraceTimeColumn].c_str(), &pEnd, 10);
		//convert trace time to simulator time 
		switch(time_unit){
			case Trace_Time_Unit::PICOSECOND:
				curFireTime = curFireTime / PicoSec2SimTimeCoeff;
				break;
			case Trace_Time_Unit::NANOSECOND:
				curFireTime = curFireTime * NanoSec2SimTimeCoeff;
				break;
			case Trace_Time_Unit::MICROSECOND:
				curFireTime = curFireTime * MicroSec2SimTimeCoeff;
				break;
			default:
				PRINT_ERROR("Unexpected Trace Time Unit!")
		}
		/* zy1024cs */
		last_req_time = curFireTime;

		Simulator->Register_sim_event(curFireTime, this);   /* （备注暂时作废）按照文件中的请求发出时间来模拟事件树中该时间什么时候发生，感觉对于何时进入请求队列或者何时进行处理的实际时间无关 */
	}
}

void IO_Flow_Trace_Based::Get_statistics(Utils::Workload_Statistics &stats, LPA_type (*Convert_host_logical_address_to_device_address)(LHA_type lha),
										 page_status_type (*Find_NVM_subunit_access_bitmap)(LHA_type lha))
{   /* 执行在host_System，但是需要preconditioning_required */
	stats.Type = Utils::Workload_Type::TRACE_BASED;
	stats.Stream_id = io_queue_id - 1; //In MQSim, there is a simple relation between stream id and the io_queue_id of NVMe
	stats.Min_LHA = start_lsa_on_device;
	stats.Max_LHA = end_lsa_on_device;
	for (int i = 0; i < MAX_ARRIVAL_TIME_HISTOGRAM + 1; i++)
	{
		stats.Write_arrival_time.push_back(0);
		stats.Read_arrival_time.push_back(0);
	}
	for (int i = 0; i < MAX_REQSIZE_HISTOGRAM_ITEMS + 1; i++)
	{
		stats.Write_size_histogram.push_back(0);
		stats.Read_size_histogram.push_back(0);
	}
	stats.Total_generated_requests = 0;
	stats.Total_accessed_lbas = 0;

	std::ifstream trace_file_temp;
	trace_file_temp.open(trace_file_path, std::ios::in);
	if (!trace_file_temp.is_open())
	{
		PRINT_ERROR("Error while opening the input trace file!")
	}

	std::string trace_line;
	char *pEnd;
	sim_time_type last_request_arrival_time = 0;
	sim_time_type sum_inter_arrival = 0;
	uint64_t sum_request_size = 0;
	std::vector<std::string> line_splitted;
	while (std::getline(trace_file_temp, trace_line))
	{
		Utils::Helper_Functions::Remove_cr(trace_line);
		line_splitted.clear();
		Utils::Helper_Functions::Tokenize(trace_line, ASCIILineDelimiter, line_splitted);
		if (line_splitted.size() != ASCIIItemsPerLine)
		{
			break;
		}
		sim_time_type prev_time = last_request_arrival_time;
		last_request_arrival_time = std::strtoull(line_splitted[ASCIITraceTimeColumn].c_str(), &pEnd, 10);
		if (last_request_arrival_time < prev_time)
		{
			PRINT_ERROR("Unexpected request arrival time: " << last_request_arrival_time << "\nMQSim expects request arrival times to be monotonic increasing in the input trace!")
		}
		sim_time_type diff = (last_request_arrival_time - prev_time) / 1000; //The arrival rate histogram is stored in the microsecond unit
		sum_inter_arrival += last_request_arrival_time - prev_time;

		unsigned int LBA_count = std::strtoul(line_splitted[ASCIITraceSizeColumn].c_str(), &pEnd, 0) / 512;
		sum_request_size += LBA_count;
		LHA_type start_LBA = std::strtoull(line_splitted[ASCIITraceAddressColumn].c_str(), &pEnd, 0) / 512;
		if (start_LBA <= (end_lsa_on_device - start_lsa_on_device))
		{
			start_LBA += start_lsa_on_device;
		}
		else
		{
			start_LBA = start_lsa_on_device + start_LBA % (end_lsa_on_device - start_lsa_on_device);
		}
		LHA_type end_LBA = start_LBA + LBA_count - 1;
		if (end_LBA > end_lsa_on_device)
		{
			end_LBA = start_lsa_on_device + (end_LBA - end_lsa_on_device) - 1;
		}

		//Address access pattern statistics
		while (start_LBA <= end_LBA)
		{
			LPA_type device_address = Convert_host_logical_address_to_device_address(start_LBA);
			page_status_type access_status_bitmap = Find_NVM_subunit_access_bitmap(start_LBA);
			if (line_splitted[ASCIITraceTypeColumn].compare(ASCIITraceWriteCode) == 0)
			{
				if (stats.Write_address_access_pattern.find(device_address) == stats.Write_address_access_pattern.end())
				{
					Utils::Address_Histogram_Unit hist;
					hist.Access_count = 1;
					hist.Accessed_sub_units = access_status_bitmap;
					stats.Write_address_access_pattern[device_address] = hist;
				}
				else
				{
					stats.Write_address_access_pattern[device_address].Access_count = stats.Write_address_access_pattern[device_address].Access_count + 1;
					stats.Write_address_access_pattern[device_address].Accessed_sub_units = stats.Write_address_access_pattern[device_address].Accessed_sub_units | access_status_bitmap;
				}

				if (stats.Read_address_access_pattern.find(device_address) != stats.Read_address_access_pattern.end())
				{
					stats.Write_read_shared_addresses.insert(device_address);
				}
			}
			else
			{
				if (stats.Read_address_access_pattern.find(device_address) == stats.Read_address_access_pattern.end())
				{
					Utils::Address_Histogram_Unit hist;
					hist.Access_count = 1;
					hist.Accessed_sub_units = access_status_bitmap;
					stats.Read_address_access_pattern[device_address] = hist;
				}
				else
				{
					stats.Read_address_access_pattern[device_address].Access_count = stats.Read_address_access_pattern[device_address].Access_count + 1;
					stats.Read_address_access_pattern[device_address].Accessed_sub_units = stats.Read_address_access_pattern[device_address].Accessed_sub_units | access_status_bitmap;
				}

				if (stats.Write_address_access_pattern.find(device_address) != stats.Write_address_access_pattern.end())
				{
					stats.Write_read_shared_addresses.insert(device_address);
				}
			}
			stats.Total_accessed_lbas++;
			start_LBA++;
			if (start_LBA > end_lsa_on_device)
			{
				start_LBA = start_lsa_on_device;
			}
		}

		//Request size statistics
		if (line_splitted[ASCIITraceTypeColumn].compare(ASCIITraceWriteCode) == 0)
		{
			if (diff < MAX_ARRIVAL_TIME_HISTOGRAM)
			{
				stats.Write_arrival_time[diff]++;
			}
			else
			{
				stats.Write_arrival_time[MAX_ARRIVAL_TIME_HISTOGRAM]++;
			}

			if (LBA_count < MAX_REQSIZE_HISTOGRAM_ITEMS)
			{
				stats.Write_size_histogram[LBA_count]++;
			}
			else
			{
				stats.Write_size_histogram[MAX_REQSIZE_HISTOGRAM_ITEMS]++;
			}
		}
		else
		{
			if (diff < MAX_ARRIVAL_TIME_HISTOGRAM)
			{
				stats.Read_arrival_time[diff]++;
			}
			else
			{
				stats.Read_arrival_time[MAX_ARRIVAL_TIME_HISTOGRAM]++;
			}

			if (LBA_count < MAX_REQSIZE_HISTOGRAM_ITEMS)
			{
				stats.Read_size_histogram[LBA_count]++;
			}
			else
			{
				stats.Read_size_histogram[(unsigned int)MAX_REQSIZE_HISTOGRAM_ITEMS]++;
			}
		}
		stats.Total_generated_requests++;
	}
	trace_file_temp.close();
	stats.Average_request_size_sector = (unsigned int)(sum_request_size / stats.Total_generated_requests);
	stats.Average_inter_arrival_time_nano_sec = sum_inter_arrival / stats.Total_generated_requests;

	stats.Initial_occupancy_ratio = initial_occupancy_ratio;
	stats.Replay_no = total_replay_no;
}
} // namespace Host_Components
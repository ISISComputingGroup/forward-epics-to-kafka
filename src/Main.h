#pragma once
#include <list>
#include <map>
#include <algorithm>
#include <mutex>
#include <atomic>
#include "MainOpt.h"
#include "TopicMapping.h"

namespace BrightnESS {
namespace ForwardEpicsToKafka {

extern std::atomic<int> g__run;

class Main {
public:
Main(MainOpt & opt);
void forward_epics_to_kafka();
int mapping_add(rapidjson::Value & mapping);
void mapping_add(TopicMappingSettings tmsettings);
void mapping_start(TopicMappingSettings tmsettings);
void mapping_remove_topic(string topic);
void mapping_list();
void forwarding_exit();
void start_some_test_mappings(int n1);
void check_instances();
void collect_and_revive_failed_mappings();
void report_stats(int started_in_current_round);
void start_mappings(int & started);
void move_failed_to_startup_queue();
void release_deleted_mappings();
void stop();
private:
int const init_pool_max = 64;
bool const do_release_memory = true;
int const memory_release_grace_time = 45;
MainOpt & main_opt;
std::list<TopicMappingSettings> tms_to_start;
std::recursive_mutex m_tms_mutex;
std::recursive_mutex m_tms_zombies_mutex;
std::recursive_mutex m_tms_failed_mutex;
std::recursive_mutex m_tms_to_start_mutex;
std::recursive_mutex m_tms_to_delete_mutex;

using RMLG = std::lock_guard<std::recursive_mutex>;
std::list<std::unique_ptr<TopicMapping>> tms;
std::list<std::unique_ptr<TopicMapping>> tms_failed;
std::list<std::unique_ptr<TopicMapping>> tms_to_delete;

// Keep them until I know how to be sure that Epics will no longer
// invoke any callbacks on them.
std::list<std::unique_ptr<TopicMapping>> tms_zombies;
Kafka::InstanceSet & kafka_instance_set;
friend class ConfigCB;

uint32_t topic_mappings_started = 0;
std::atomic<int32_t> forwarding_run {1};
};

}
}

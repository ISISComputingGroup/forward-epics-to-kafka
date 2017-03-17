#include "MainOpt.h"

#ifdef _MSC_VER
	#include "wingetopt.h"
#elif _AIX
	#include <unistd.h>
#else
	#include <getopt.h>
#endif

#include <rapidjson/writer.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/schema.h>
#include "helper.h"
#include "logger.h"
#include "blobs.h"
#include "SchemaRegistry.h"
#include "git_commit_current.h"

namespace BrightnESS {
namespace ForwardEpicsToKafka {

using std::string;
using std::vector;


MainOpt::MainOpt() {
	set_broker("localhost:9092");
}


void MainOpt::set_broker(string broker) {
	brokers.clear();
	auto a = split(broker, ",");
	for (auto & x : a) {
		uri::URI u1;
		u1.require_host_slashes = false;
		u1.init(x);
		brokers.push_back(u1);
	}
}


std::string MainOpt::brokers_as_comma_list() const {
	std::string s1;
	int i1 = 0;
	for (auto & x : brokers) {
		if (i1) s1 += ",";
		s1 += x.host_port;
		++i1;
	}
	return s1;
}


int MainOpt::parse_json_file(string config_file) {
	if (config_file == "") {
		LOG(3, "given config filename is empty");
		return -1;
	}
	this->config_file = config_file;
	using namespace rapidjson;
	Document schema_;
	try {
		auto & s = blobs::schema_config_global_json;
		schema_.Parse(s.data(), s.size());
	}
	catch (...) {
		LOG(7, "ERROR schema is not valid!");
		return -2;
	}
	if (schema_.HasParseError()) {
		LOG(7, "ERROR could not parse schema");
		return -3;
	}
	SchemaDocument schema(schema_);

	// Parse the JSON configuration and extract parameters.
	// Currently, these parameters take precedence over what is given on the command line.
	FILE * f1 = fopen(config_file.c_str(), "rb");
	if (not f1) {
		LOG(3, "can not find the requested config-file");
		return -4;
	}
	int const N1 = 16000;
	char buf1[N1];
	FileReadStream is(f1, buf1, N1);
	json = std::make_shared<Document>();
	auto & d = * json;
	d.ParseStream(is);
	fclose(f1);
	f1 = 0;

	if (d.HasParseError()) {
		LOG(7, "ERROR configuration is not well formed");
		return -5;
	}
	SchemaValidator vali(schema);
	if (!d.Accept(vali)) {
		StringBuffer sb1, sb2;
		vali.GetInvalidSchemaPointer().StringifyUriFragment(sb1);
		vali.GetInvalidDocumentPointer().StringifyUriFragment(sb2);
		LOG(7, "ERROR command message schema validation:  Invalid schema: {}  keyword: {}",
			sb1.GetString(),
			vali.GetInvalidSchemaKeyword()
		);
		if (std::string("additionalProperties") == vali.GetInvalidSchemaKeyword()) {
			LOG(7, "Sorry, you have probably specified more properties than what is allowed by the schema.");
		}
		LOG(7, "ERROR configuration is not valid");
		return -6;
	}
	vali.Reset();

	{
		auto & v = d.FindMember("broker")->value;
		if (v.IsString()) {
			set_broker(v.GetString());
		}
	}
	{
		auto & v = d.FindMember("kafka")->value;
		if (v.IsObject()) {
			auto & v2 = v.FindMember("broker")->value;
			if (v2.IsObject()) {
				for (auto & x : v2.GetObject()) {
					auto const & n = x.name.GetString();
					if (strncmp("___", n, 3) == 0) {
						// ignore
					}
					else {
						if (x.value.IsString()) {
							auto const & v = x.value.GetString();
							LOG(6, "kafka broker config {}: {}", n, v);
							broker_opt.conf_strings[n] = v;
						}
						else if (x.value.IsInt()) {
							auto const & v = x.value.GetInt();
							LOG(6, "kafka broker config {}: {}", n, v);
							broker_opt.conf_ints[n] = v;
						}
						else {
							LOG(3, "ERROR can not understand option: {}", n);
						}
					}
				}
			}
		}
	}
	return 0;
}



std::pair<int, std::unique_ptr<MainOpt>> parse_opt(int argc, char ** argv) {
	std::pair<int, std::unique_ptr<MainOpt>> ret {0, std::unique_ptr<MainOpt>(new MainOpt)};
	auto & opt = *ret.second;
	static struct option long_options[] = {
		{"help",                            no_argument,              0, 'h'},
		{"broker-config",                   required_argument,        0,  0 },
		{"broker",                          required_argument,        0,  0 },
		{"kafka-gelf",                      required_argument,        0,  0 },
		{"graylog-logger-address",          required_argument,        0,  0 },
		{"config-file",                     required_argument,        0,  0 },
		{"log-file",                        required_argument,        0,  0 },
		{"forwarder-ix",                    required_argument,        0,  0 },
		{"write-per-message",               required_argument,        0,  0 },
		{"teamid",                          required_argument,        0,  0 },
		{0, 0, 0, 0},
	};
	int option_index = 0;
	bool getopt_error = false;
	while (true) {
		int c = getopt_long(argc, argv, "hvQ", long_options, &option_index);
		//LOG(2, "c getopt {}", c);
		if (c == -1) break;
		if (c == '?') {
			//LOG(2, "option argument missing");
			getopt_error = true;
		}
		if (false) {
			if (c == 0) {
				printf("at long  option %d [%1c] %s\n", option_index, c, long_options[option_index].name);
			}
			else {
				printf("at short option %d [%1c]\n", option_index, c);
			}
		}
		switch (c) {
		case 'h':
			opt.help = true;
			break;
		case 'v':
			log_level = std::min(9, log_level + 1);
			break;
		case 'Q':
			log_level = std::max(0, log_level - 1);
			break;
		case 0:
			auto lname = long_options[option_index].name;
			// long option without short equivalent:
			if (std::string("help") == lname) {
				opt.help = true;
			}
			if (std::string("config-file") == lname) {
				opt.parse_json_file(optarg);
			}
			if (std::string("log-file") == lname) {
				opt.log_file = optarg;
			}
			if (std::string("broker-config") == lname) {
				uri::URI u1;
				u1.init(optarg);
				u1.default_port(9092);
				opt.broker_config = u1;
			}
			if (std::string("broker") == lname) {
				opt.set_broker(optarg);
			}
			if (std::string("kafka-gelf") == lname) {
				opt.kafka_gelf = optarg;
			}
			if (std::string("graylog-logger-address") == lname) {
				opt.graylog_logger_address = optarg;
			}
			if (std::string("forwarder-ix") == lname) {
				opt.forwarder_ix = strtol(optarg, nullptr, 10);
			}
			if (std::string("write-per-message") == lname) {
				opt.write_per_message = strtol(optarg, nullptr, 10);
			}
			if (std::string("teamid") == lname) {
				opt.teamid = strtoul(optarg, nullptr, 0);
			}
		}
	}
	if (optind < argc) {
		LOG(6, "Left-over commandline options:");
		for (int i1 = optind; i1 < argc; ++i1) {
			LOG(6, "{:2} {}", i1, argv[i1]);
		}
	}
	if (getopt_error) {
		opt.help = true;
		ret.first = 1;
	}
	if (opt.help) {
		fmt::print(
			"forward-epics-to-kafka-0.0.1 {:.7} (ESS, BrightnESS)\n"
			"  Contact: dominik.werder@psi.ch\n\n",
			GIT_COMMIT
		);
		fmt::print(
			"Forwards EPICS process variables to Kafka topics.\n"
			"\n"
			"forward-epics-to-kafka\n"
			"  --help, -h\n"
			"\n"
			"  --config-file                     filename\n"
			"      Configuration file in JSON format.\n"
			"      To overwrite the options in config-file, specify them later on the command line.\n"
			"\n"
			"  --broker-config                   //host[:port]/topic\n"
			"      Kafka brokers to connect with for configuration updates.\n"
			"\n"
			"  --broker                          host:port,host:port,...\n"
			"      Kafka brokers to connect with for configuration updates\n"
			"      Default: {}\n"
			"\n"
			"  --kafka-gelf                      kafka://host[:port]/topic\n"
			"\n"
			"  --graylog-logger-address          host:port\n"
			"      Log to Graylog via graylog_logger library.\n"
			"\n"
			"  -v\n"
			"      Decrease log_level by one step.  Default log_level is 3.\n"
			"  -Q\n"
			"      Increase log_level by one step.\n"
			"\n",
			opt.brokers_as_comma_list()
		);
		ret.first = 1;
	}

	return ret;
}


void MainOpt::init_logger() {
	if (kafka_gelf != "") {
		BrightnESS::uri::URI uri(kafka_gelf);
		log_kafka_gelf_start(uri.host, uri.topic);
		LOG(3, "Enabled kafka_gelf: //{}/{}", uri.host, uri.topic);
	}
	if (graylog_logger_address != "") {
		fwd_graylog_logger_enable(graylog_logger_address);
	}
}


}
}
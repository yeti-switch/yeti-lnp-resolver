#include "prometheus_exporter.h"
#include "log.h"
#include "cfg.h"

#define METRICS_PREFIX "yeti_lnp_resolver_"

::prometheus::Labels static_labels;

extern int label_func(cfg_t *cfg, cfg_opt_t *, int argc, const char **argv)
{
    if(argc != 2) {
        cfg_error(cfg, "label must have 2 arguments");
        return 1;
    }
    std::string option, value;
    switch(argc)
    {
    case 0:
        return 1;
    case 2:
        value = argv[1];
        /* fall through */
    case 1:
        option = argv[0];
        break;
    }

    static_labels.emplace(option, value);

    return 0;
}

void PrometheusExporter::start()
{
	// stop if exporter already started
	stop();

	// create an http server
	string host = cfg.prometheus.host;
	unsigned int port = cfg.prometheus.port;
	string bind_address = host + ":" + to_string(port);

	info("bind %s", bind_address.c_str());

	try {
		exposer = make_shared<Exposer>(bind_address);
	} catch(std::string &s){
		err("%s",s.c_str());
		return;
	} catch(std::exception &e) {
		err("%s\n",e.what());
		return;
	}

	// create a metrics registry
	registry = make_shared<Registry>();

	// create driver_requests_count
	driver_requests_count = &BuildCounter()
		.Name(METRICS_PREFIX "driver_requests_count")
		.Help("The total number of requests attempts")
		.Labels(static_labels)
		.Register(*registry);

	// create driver_requests_failed
	driver_requests_failed = &BuildCounter()
		.Name(METRICS_PREFIX "driver_requests_failed")
		.Help("Failed requests count")
		.Labels(static_labels)
		.Register(*registry);

	// create driver_requests_finished
	driver_requests_finished = &BuildCounter()
		.Name(METRICS_PREFIX "driver_requests_finished")
		.Help("Finished requests count")
		.Labels(static_labels)
		.Register(*registry);

	// create driver_requests_time
	driver_requests_time = &BuildCounter()
		.Name(METRICS_PREFIX "driver_requests_time")
		.Help("Accumulated request processing time in ms")
		.Labels(static_labels)
		.Register(*registry);

	// ask the exposer to scrape the registry on incoming HTTP requests
	exposer->RegisterCollectable(registry);

	info("started");
}

void PrometheusExporter::stop()
{
	exposer = NULL;
	registry = NULL;
	driver_requests_count = NULL;
	driver_requests_failed = NULL;
	driver_requests_time = NULL;
}


void PrometheusExporter::driver_requests_count_increment(
	const string &type,
	CDriverCfg::CfgUniqId_t id)
{
	std::lock_guard<std::mutex> lock{mutex_};

	if (driver_requests_count != nullptr)
		driver_requests_count->Add(
			{ {"type", type},
			  {"id", std::to_string(id) }
			}).Increment();
}

void PrometheusExporter::driver_requests_failed_increment(
	const string &type,
	CDriverCfg::CfgUniqId_t id)
{
	std::lock_guard<std::mutex> lock{mutex_};

	if (driver_requests_failed != nullptr)
		driver_requests_failed->Add(
			{ {"type", type},
			  {"id", std::to_string(id)}
			}).Increment();
}

void PrometheusExporter::driver_requests_finished_increment(
	const string &type,
	CDriverCfg::CfgUniqId_t id,
	const double time_consumed)
{
	prometheus::Labels l({
		{"type", type},
		{"id", std::to_string(id) }
	});

	std::lock_guard<std::mutex> lock{mutex_};

	if (driver_requests_finished != nullptr)
		driver_requests_finished->Add(l).Increment();

	if (driver_requests_time != nullptr)
		driver_requests_time->Add(l).Increment(time_consumed);
}

void PrometheusExporter::driver_init_metrics(
	const string &type,
	CDriverCfg::CfgUniqId_t id)
{
	prometheus::Labels l({
		{"type", type},
		{"id", std::to_string(id) }
	});

	std::lock_guard<std::mutex> lock{mutex_};

	if (driver_requests_count != nullptr)
		driver_requests_count->Add(l);

	if (driver_requests_failed != nullptr)
		driver_requests_failed->Add(l);

	if (driver_requests_finished != nullptr)
		driver_requests_finished->Add(l);

	if (driver_requests_time != nullptr)
		driver_requests_time->Add(l);
}

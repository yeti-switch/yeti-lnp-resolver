#include "prometheus_exporter.h"
#include "log.h"
#include "cfg.h"

#define METRICS_PREFIX "yeti_lnp_resolver_"

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
	regisrty = make_shared<Registry>();

	// create driver_requests_count
	driver_requests_count = &BuildCounter()
		.Name(METRICS_PREFIX "driver_requests_count")
		.Help("The total number of requests attempts")
		.Register(*regisrty);

	// create driver_requests_failed
	driver_requests_failed = &BuildCounter()
		.Name(METRICS_PREFIX "driver_requests_failed")
		.Help("Failed requests count")
		.Register(*regisrty);

	// create driver_requests_finished
	driver_requests_finished = &BuildCounter()
		.Name(METRICS_PREFIX "driver_requests_finished")
		.Help("Finished requests count")
		.Register(*regisrty);

	// create driver_requests_time
	driver_requests_time = &BuildCounter()
		.Name(METRICS_PREFIX "driver_requests_time")
		.Help("Accumulated request processing time in ms")
		.Register(*regisrty);

	// ask the exposer to scrape the registry on incoming HTTP requests
	exposer->RegisterCollectable(regisrty);

	info("started");
}

void PrometheusExporter::stop()
{
	exposer = NULL;
	regisrty = NULL;
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

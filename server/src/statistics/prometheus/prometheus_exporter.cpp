#include "prometheus_exporter.h"
#include "log.h"
#include "cfg.h"

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
	.Name("driver_requests_count")
	.Help("The total number of requests")
	.Register(*regisrty);

	// create driver_requests_failed
	driver_requests_failed = &BuildCounter()
	.Name("driver_requests_failed")
	.Help("Failed requests count")
	.Register(*regisrty);

	// create driver_requests_time
	driver_requests_time = &BuildCounter()
	.Name("driver_requests_time")
	.Help("Accumulated request processing time in ms")
	.Register(*regisrty);

	// ask the exposer to scrape the registry on incoming HTTP requests
	exposer->RegisterCollectable(regisrty);

	info("did start");
}

void PrometheusExporter::stop()
{
	exposer = NULL;
	regisrty = NULL;
	driver_requests_count = NULL;
	driver_requests_failed = NULL;
	driver_requests_time = NULL;
}


void PrometheusExporter::driver_requests_count_increment(string driver) {
	std::lock_guard<std::mutex> lock{mutex_};

	if (driver_requests_count != NULL)
		driver_requests_count->Add({{"driver", driver}}).Increment();
}

void PrometheusExporter::driver_requests_failed_increment(string driver) {
	std::lock_guard<std::mutex> lock{mutex_};

	if (driver_requests_failed != NULL)
		driver_requests_failed->Add({{"driver", driver}}).Increment();
}

void PrometheusExporter::driver_requests_time_increment(string driver, const double val) {
	std::lock_guard<std::mutex> lock{mutex_};

	if (driver_requests_time != NULL)
		driver_requests_time->Add({{"driver", driver}}).Increment(val);
}

#include "prometheus_exporter.h"
#include "log.h"

void PrometheusExporter::start(string ip, uint16_t port)
{
	// stop if exporter already started
	stop();

	// create an http server
	string bind_address = ip + ":" + to_string(port);
	exposer = make_shared<Exposer>(bind_address);

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

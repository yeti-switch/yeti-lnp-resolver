#ifndef PROMEXPORTER_H
#define PROMEXPORTER_H

#include <string>

#include "singleton.h"
#include "prometheus/counter.h"
#include "prometheus/exposer.h"
#include "prometheus/family.h"
#include "prometheus/registry.h"

class PrometheusExporter;
typedef singleton<PrometheusExporter> prometheus_exporter;

using namespace std;
using namespace prometheus;
using namespace detail;

class PrometheusExporter
{
public:
	void start(string ip, uint16_t port);
	void stop();

	void driver_requests_count_increment(string driver);
	void driver_requests_failed_increment(string driver);
	void driver_requests_time_increment(string driver, const double val);

private:
	shared_ptr<Exposer> exposer;
	shared_ptr<Registry> regisrty;

	mutable std::mutex mutex_;

	Family<Counter>* driver_requests_count;
	Family<Counter>* driver_requests_failed;
	Family<Counter>* driver_requests_time;
};


#endif // PROMEXPORTER_H

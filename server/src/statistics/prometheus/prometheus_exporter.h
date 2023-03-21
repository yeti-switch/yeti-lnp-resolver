#pragma once

#include <string>

#include "singleton.h"
#include "prometheus/counter.h"
#include "prometheus/exposer.h"
#include "prometheus/family.h"
#include "prometheus/registry.h"

#include "drivers/Driver.h"

class PrometheusExporter;
typedef singleton<PrometheusExporter> prometheus_exporter;

using namespace std;
using namespace prometheus;
using namespace detail;

class PrometheusExporter
{
public:
	void start();
	void stop();

	void driver_requests_count_increment(
		const string &type, CDriverCfg::CfgUniqId_t id);

	void driver_requests_failed_increment(
		const string &type, CDriverCfg::CfgUniqId_t id);

	void driver_requests_finished_increment(
		const string &type, CDriverCfg::CfgUniqId_t id,
		const double time_consumed);

	void driver_init_metrics(
		const string &type,
		CDriverCfg::CfgUniqId_t id);

private:
	shared_ptr<Exposer> exposer;
	shared_ptr<Registry> regisrty;

	mutable std::mutex mutex_;

	Family<Counter>* driver_requests_count;
	Family<Counter>* driver_requests_failed;
	Family<Counter>* driver_requests_finished;
	Family<Counter>* driver_requests_time;
};
